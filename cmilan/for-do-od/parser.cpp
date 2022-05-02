#include "parser.h"
#include <sstream>
#include <queue>

//Выполняем синтаксический разбор блока program. Если во время разбора не обнаруживаем 
//никаких ошибок, то выводим последовательность команд стек-машины
void Parser::parse()
{
	program(); 
	if(!error_) {
		codegen_->flush();
	}
}

void Parser::program()
{
	mustBe(T_BEGIN);
	statementList();
	mustBe(T_END);
	codegen_->emit(STOP);
}

/**
 * 匹配结尾运算符：
 *  - END
 *  - OD
 *  - ELSE
 *  - FI
 */
void Parser::statementList()
{
    //	Если список операторов пуст, очередной лексемой будет одна из возможных "закрывающих скобок": END, OD, ELSE, FI.
    //	В этом случае результатом разбора будет пустой блок (его список операторов равен null).
    //	Если очередная лексема не входит в этот список, то ее мы считаем началом оператора и вызываем метод statement.
    //  Признаком последнего оператора является отсутствие после оператора точки с запятой.
    //  如果运算符列表为空，下一个标记将是可能的 "结束括号 "之一：END、OD、ELSE、FI。
    //  在这种情况下，解析的结果将是一个空块（其运算符列表为 null）。
    //  如果下一个标记不在这个列表中，我们认为它是一个运算符的开始，并调用语句方法。
    //  【重点】最后一个声明的标志是运算符后面没有分号。
	if(see(T_END) || see(T_OD) || see(T_ELSE) || see(T_FI)) {
		return;
	}
	else {
		bool more = true;
		while(more) {
			statement();
			more = match(T_SEMICOLON);
		}
	}
}

/**
 * 运算符分析：
 *  - 变量
 *  - IF
 *  - WHILE
 *  - WRITE
 */
void Parser::statement()
{
    /*
	 * Если встречаем переменную, то запоминаем ее адрес или добавляем новую если не встретили.
	 * Следующей лексемой должно быть присваивание. Затем идет блок expression, который возвращает значение на вершину стека.
	 * Записываем это значение по адресу нашей переменной
     * 如果我们遇到一个变量，我们会记住它的地址，如果没有的话，就添加一个新的地址。
     * 下一个词素应该是一个任务。然后是表达式块，它向栈顶返回一个值。
     * 把这个值写到我们的变量地址中
     * */
	if(see(T_IDENTIFIER)) {
		int varAddress = findOrAddVariable(scanner_->getStringValue());
		next();
		mustBe(T_ASSIGN);
		expression();
		codegen_->emit(STORE, varAddress);
	}

    /*
     * Если встретили IF, то затем должно следовать условие. На вершине стека лежит 1 или 0 в зависимости от выполнения условия.
     * Затем зарезервируем место для условного перехода JUMP_NO к блоку ELSE (переход в случае ложного условия). Адрес перехода
     * станет известным только после того, как будет сгенерирован код для блока THEN.
     * 如果遇到了IF，那么条件必须随之而来。在堆栈的顶部是1或0，取决于是否满足条件。
     * 然后为条件性过渡JUMP_NO到ELSE块保留空间（在错误条件下的过渡）。跳转地址
     * 只有在THEN块的代码生成后才知道。
     * */
    else if(match(T_IF)) {
        relation();  // 用于比较表达式，结果以 0 或 1 放置在栈的顶端

        // 生成一个空的地址
        int jumpNoAddress = codegen_->reserve();

        // 在 IF 后面必须是运算符 THEN
        mustBe(T_THEN);
        // 判断 THEN 之后的运算符是不是 END、OD、ELSE、FI 之一的
        statementList();

        if(match(T_ELSE)) {  // 如果匹配到了 ELSE

            // Если есть блок ELSE, то чтобы не выполнять его в случае выполнения THEN, зарезервируем место для команды JUMP в конец этого блока
            // 如果有一个 ELSE 块，为了在 THEN 被执行时不执行它，我们在这个块的末尾为 JUMP 命令保留空间
            int jumpAddress = codegen_->reserve();

            // Заполним зарезервированное место после проверки условия инструкцией перехода в начало блока ELSE.
            // 如果 IF 后面的条件为【JUMP_NO：0】，则将之前保留的空间（jumpNoAddress），置为跳转到 ELSE
            codegen_->emitAt(jumpNoAddress, JUMP_NO, codegen_->getCurrentAddress());
            statementList();

            // Заполним второй адрес инструкцией перехода в конец условного блока ELSE.
            // 在ELSE条件块的末尾，用过渡指令填入第二个地址。
            codegen_->emitAt(jumpAddress, JUMP, codegen_->getCurrentAddress());
        }
        else {  // 如果当前IF 语句里没有 ELSE 语句
            //Если блок ELSE отсутствует, то в зарезервированный адрес после проверки условия будет записана
            //инструкция условного перехода в конец оператора IF...THEN
            // 如果没有ELSE块，在检查条件后，一条条件跳转指令将被写入IF...THEN语句末尾的保留地址中
            codegen_->emitAt(jumpNoAddress, JUMP_NO, codegen_->getCurrentAddress());
        }

        mustBe(T_FI);
    }

	else if(match(T_WHILE)) {
		//запоминаем адрес начала проверки условия.
        // 条件检查的起始地址
		int conditionAddress = codegen_->getCurrentAddress();
		relation();  // 条件解析

		//резервируем место под инструкцию условного перехода для выхода из цикла.
        // 为退出 WHILE 循环的条件性跳转指令保留空间
		int jumpNoAddress = codegen_->reserve();
		mustBe(T_DO);
		statementList();
		mustBe(T_OD);

		//переходим по адресу проверки условия
        // 转到条件检查地址，其中 JUMP 为无条件跳转
		codegen_->emit(JUMP, conditionAddress);

		//заполняем зарезервированный адрес инструкцией условного перехода на следующий за циклом оператор.
        // 如果不满足条件即跳出循环：用循环后的运算符上的条件跳转指令填入保留地址。
		codegen_->emitAt(jumpNoAddress, JUMP_NO, codegen_->getCurrentAddress());
	}

    else if (match(T_FOR))
    {
        //запоминаем адрес начала проверки условия.
        // 条件检查的起始地址
//        int conditionAddress = codegen_->getCurrentAddress();

        // FOR 后面一个应该是 `变量 :=`
        if (!see(T_IDENTIFIER))  // FOR 后面的变量
        {
            reportError("[ERROR] `FOR` should be followed by the variable");
            return;
        }

        string startValueName = scanner_->getStringValue();
        int firstVarAddress = findOrAddVariable(startValueName);  // 初始变量名的位置
        next();
        mustBe(T_ASSIGN);  // `:=`
        expression();
//        codegen_->emit(STORE, firstVarAddress);


        /** 开始检测逗号后面的值，知道检测不到逗号
         * 将检测到的新值全部放入到栈中
         */
        queue<int> parts;
        while (match(T_COMMA))
        {
//            int varAddress = findOrAddVariable(startValueName);  // 变量名的位置
//            int varAddress = findOrAddVariable(scanner_->getIntValue() + 1);  // 变量名的位置

            expression();
//            codegen_->emit(STORE, firstVarAddress);
            parts.push(firstVarAddress);
        }


        // for 循环开始的地方
        int conditionAddress = codegen_->getCurrentAddress();

        // 为退出 FOR 循环的条件性跳转指令保留空间
//        int jumpNoAddress = codegen_->reserve();



        // 在 FOR 循环开始之前，将栈顶部的变量值加载
        codegen_->emit(STORE, firstVarAddress);

        mustBe(T_DO);
        statementList();
        mustBe(T_OD);

//        if (!parts.empty())
//        {
//            int nextVarAddress = parts.front();
//            parts.pop();
//            codegen_->emit(JUMP, nextVarAddress);
//        }
//        else
//        {
//            codegen_->emit(JUMP, jumpNoAddress);
//        }
//        codegen_->emit(STORE, firstVarAddress);
        codegen_->emit(JUMP, conditionAddress);

        // 为退出 FOR 循环的条件性跳转指令保留空间
        int jumpNoAddress = codegen_->reserve();
    }


	else if(match(T_WRITE)) {
		mustBe(T_LPAREN);
		expression();
		mustBe(T_RPAREN);
		codegen_->emit(PRINT);
	}
	else {
		reportError("statement expected.");
	}
}

/**
 * 解析算数表达式 —— 加减运算
 */
void Parser::expression()
{

	 /*
         Арифметическое выражение описывается следующими правилами: <expression> -> <term> | <term> + <term> | <term> - <term>
         При разборе сначала смотрим первый терм, затем анализируем очередной символ. Если это '+' или '-', 
		 удаляем его из потока и разбираем очередное слагаемое (вычитаемое). Повторяем проверку и разбор очередного 
		 терма, пока не встретим за термом символ, отличный от '+' и '-'
     */

	term();
	while(see(T_ADDOP)) {
		Arithmetic op = scanner_->getArithmeticValue();
		next();
		term();

		if(op == A_PLUS) {
			codegen_->emit(ADD);
		}
		else {
			codegen_->emit(SUB);
		}
	}
}

/**
 * 解析算数表达式 —— 乘除运算
 */
void Parser::term()
{
	 /*  
		 Терм описывается следующими правилами: <expression> -> <factor> | <factor> + <factor> | <factor> - <factor>
         При разборе сначала смотрим первый множитель, затем анализируем очередной символ. Если это '*' или '/', 
		 удаляем его из потока и разбираем очередное слагаемое (вычитаемое). Повторяем проверку и разбор очередного 
		 множителя, пока не встретим за ним символ, отличный от '*' и '/' 
	*/
	factor();
	while(see(T_MULOP)) {
		Arithmetic op = scanner_->getArithmeticValue();
		next();
		factor();

		if(op == A_MULTIPLY) {
			codegen_->emit(MULT);
		}
		else {
			codegen_->emit(DIV);
		}
	}
}

/**
 * 解析算数表达式 —— 乘法分析
 */
void Parser::factor()
{
	/*
		Множитель описывается следующими правилами:
		<factor> -> number | identifier | -<factor> | (<expression>) | READ
	*/
	if(see(T_NUMBER)) {
		int value = scanner_->getIntValue();
		next();
		codegen_->emit(PUSH, value);
		//Если встретили число, то преобразуем его в целое и записываем на вершину стека
	}
	else if(see(T_IDENTIFIER)) {
		int varAddress = findOrAddVariable(scanner_->getStringValue());
		next();
		codegen_->emit(LOAD, varAddress);
		//Если встретили переменную, то выгружаем значение, лежащее по ее адресу, на вершину стека 
	}
	else if(see(T_ADDOP) && scanner_->getArithmeticValue() == A_MINUS) {
		next();
		factor();
		codegen_->emit(INVERT);
		//Если встретили знак "-", и за ним <factor> то инвертируем значение, лежащее на вершине стека
	}
	else if(match(T_LPAREN)) {
		expression();
		mustBe(T_RPAREN);
		//Если встретили открывающую скобку, тогда следом может идти любое арифметическое выражение и обязательно
		//закрывающая скобка.
	}
	else if(match(T_READ)) {
		codegen_->emit(INPUT);
		//Если встретили зарезервированное слово READ, то записываем на вершину стека идет запись со стандартного ввода
	}
	else {
		reportError("expression expected.");
	}
}

/**
 * 条件解析
 */
void Parser::relation()
{
	//Условие сравнивает два выражения по какому-либо из знаков. Каждый знак имеет свой номер. В зависимости от 
	//результата сравнения на вершине стека окажется 0 или 1.
	expression();
	if(see(T_CMP)) {
		Cmp cmp = scanner_->getCmpValue();
		next();
		expression();
		switch(cmp) {
			//для знака "=" - номер 0
			case C_EQ:
				codegen_->emit(COMPARE, 0);
				break;
			//для знака "!=" - номер 1
			case C_NE:
				codegen_->emit(COMPARE, 1);
				break;
			//для знака "<" - номер 2
			case C_LT:
				codegen_->emit(COMPARE, 2);
				break;
			//для знака ">" - номер 3
			case C_GT:
				codegen_->emit(COMPARE, 3);
				break;
			//для знака "<=" - номер 4
			case C_LE:
				codegen_->emit(COMPARE, 4);
				break;
			//для знака ">=" - номер 5
			case C_GE:
				codegen_->emit(COMPARE, 5);
				break;
		};
	}
	else {
		reportError("comparison operator expected.");
	}
}

int Parser::findOrAddVariable(const string& var)
{
	VarTable::iterator it = variables_.find(var);
	if(it == variables_.end()) {
		variables_[var] = lastVar_;
		return lastVar_++;
	}
	else {
		return it->second;
	}
}

void Parser::mustBe(Token t)
{
	if(!match(t)) {
		error_ = true;

		// Подготовим сообщение об ошибке
		std::ostringstream msg;
		msg << tokenToString(scanner_->token()) << " found while " << tokenToString(t) << " expected.";
		reportError(msg.str());

		// Попытка восстановления после ошибки.
		recover(t);
	}
}

void Parser::recover(Token t)
{
	while(!see(t) && !see(T_EOF)) {
		next();
	}

	if(see(t)) {
		next();
	}
}
