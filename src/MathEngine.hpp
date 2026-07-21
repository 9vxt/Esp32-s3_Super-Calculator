#ifndef MATH_ENGINE_HPP
#define MATH_ENGINE_HPP

#include <Arduino.h>
#include <vector>
#include <string>
#include <stack>
#include <cmath>

class MathEngine {
private:
    int getPrecedence(char op) {
        if (op == '+' || op == '-') return 1;
        if (op == '*' || op == '/') return 2;
        if (op == '^') return 3;
        return 0;
    }

    bool isOperator(char c) {
        return c == '+' || c == '-' || c == '*' || c == '/' || c == '^';
    }

public:
    double evaluate(const std::string& expression) {
        std::stack<double> values;
        std::stack<char> ops;

        for (size_t i = 0; i < expression.length(); i++) {
            if (expression[i] == ' ') continue;

            if (isdigit(expression[i]) || expression[i] == '.') {
                std::string valStr = "";
                while (i < expression.length() && (isdigit(expression[i]) || expression[i] == '.')) {
                    valStr += expression[i];
                    i++;
                }
                i--;
                values.push(std::stod(valStr));
            }
            else if (expression[i] == '(') {
                ops.push(expression[i]);
            }
            else if (expression[i] == ')') {
                while (!ops.empty() && ops.top() != '(') {
                    double val2 = values.top(); values.pop();
                    double val1 = values.top(); values.pop();
                    char op = ops.top(); ops.pop();
                    values.push(applyOp(val1, val2, op));
                }
                if (!ops.empty()) ops.pop();
            }
            else if (isOperator(expression[i])) {
                while (!ops.empty() && getPrecedence(ops.top()) >= getPrecedence(expression[i])) {
                    double val2 = values.top(); values.pop();
                    double val1 = values.top(); values.pop();
                    char op = ops.top(); ops.pop();
                    values.push(applyOp(val1, val2, op));
                }
                ops.push(expression[i]);
            }
        }

        while (!ops.empty()) {
            double val2 = values.top(); values.pop();
            double val1 = values.top(); values.pop();
            char op = ops.top(); ops.pop();
            values.push(applyOp(val1, val2, op));
        }

        return values.empty() ? 0.0 : values.top();
    }

    double applyOp(double a, double b, char op) {
        switch (op) {
            case '+': return a + b;
            case '-': return a - b;
            case '*': return a * b;
            case '/': return b != 0 ? a / b : 0.0;
            case '^': return pow(a, b);
            default: return 0;
        }
    }
};

#endif