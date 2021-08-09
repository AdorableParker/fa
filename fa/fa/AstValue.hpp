#ifndef __AST_VALUE_HPP__
#define __AST_VALUE_HPP__



#include <optional>
#include <string>

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>

#include <FaParser.h>
#include "ValueBuilder.hpp"
#include "TypeMap.hpp"
#include "Log.hpp"



class AstValue {
	enum class AstObjectType { None, Value, Var, Func, TypeStr };

public:
	AstValue () {}
	AstValue (std::nullopt_t) {}
	AstValue (std::shared_ptr<ValueBuilder> _value_builder, FaParser::LiteralContext *_literal) {
		if (_literal->BoolLiteral ()) {
			m_value_type = "bool";
		} else if (_literal->IntLiteral ()) {
			m_value_type = "int";
		} else if (_literal->FloatLiteral ()) {
			m_value_type = "float";
		} else if (_literal->String1Literal ()) {
			m_value_type = "string";
		} else {
			LOG_ERROR (_literal->start, "δ֪��������");
			return;
		}
		std::optional<std::tuple<llvm::Value *, std::string>> _oval = _value_builder->Build (m_value_type, _literal->getText (), _literal->start);
		if (_oval.has_value ()) {
			m_type = AstObjectType::Value;
			std::tie (m_value, m_value_type) = _oval.value ();
		}
	}
	AstValue (llvm::AllocaInst *_var, std::string _value_type): m_type (_var ? AstObjectType::Var : AstObjectType::None), m_value (_var) {}
	AstValue (llvm::Value *_value, std::string _value_type): m_type (_value ? AstObjectType::Value : AstObjectType::None), m_value (_value) {}
	AstValue (llvm::Function *_func, std::string _value_type): m_type (_func ? AstObjectType::Func : AstObjectType::None), m_func (_func) {}
	AstValue &operator= (const llvm::AllocaInst *_val) { AstValue _o { const_cast<llvm::AllocaInst *> (_val) }; return operator= (_o); }
	AstValue &operator= (const llvm::Value *_val) { AstValue _o { const_cast<llvm::Value *> (_val) }; return operator= (_o); }
	AstValue &operator= (const llvm::Function *_val) { AstValue _o { const_cast<llvm::Function *> (_val) }; return operator= (_o); }
	AstValue &operator= (const AstValue &_o) {
		m_type = _o.m_type;
		m_value = _o.m_value;
		m_func = _o.m_func;
		return *this;
	}

	bool IsValid () const { return m_type != AstObjectType::None; }
	bool IsValue () const { return m_type == AstObjectType::Value || m_type == AstObjectType::Var; }
	bool IsVariable () const { return m_type == AstObjectType::Var; }
	bool IsFunction () const { return m_type == AstObjectType::Func; }
	bool IsTypeStr () const { return m_type == AstObjectType::Value || m_type == AstObjectType::Var || m_type == AstObjectType::TypeStr; }

	llvm::Value *Value (llvm::IRBuilder<> &_builder) {
		if (m_type == AstObjectType::Value) {
			return m_value;
		} else if (m_type == AstObjectType::Var) {
			return _builder.CreateLoad (m_value);
		} else {
			return nullptr;
		}
	}
	llvm::CallInst *FunctionCall (llvm::IRBuilder<> &_builder, std::vector<llvm::Value *> &_args) {
		if (m_type != AstObjectType::Func || m_func == nullptr)
			return nullptr;
		return _builder.CreateCall (m_func, _args);
	}
	AstValue DoOper1 (llvm::IRBuilder<> &_builder, std::shared_ptr<ValueBuilder> _value_builder, std::string _op, antlr4::Token *_t) {
		if (!IsValue ())
			return std::nullopt;
		llvm::Value *_tmp = Value (_builder);
		if (_op == "+") {
			return *this;
		} else {
			std::string _typestr = TypeMap::GetTypeName (m_value->getType ());
			if (_op == "-") {
				std::optional<llvm::Value *> _tmp2 = _value_builder->Build (_typestr, "0", _t);
				if (!_tmp2.has_value ())
					return std::nullopt;
				return _builder.CreateSub (_tmp2.value (), _tmp);
			} else if (_op == "++" || _op == "--") {
				std::optional<llvm::Value *> _tmp2 = _value_builder->Build (_typestr, "1", _t);
				if (!_tmp2.has_value ())
					return std::nullopt;
				AstValue _v;
				if (_op == "++") {
					_v = _builder.CreateAdd (Value (_builder), _tmp2.value ());
				} else {
					_v = _builder.CreateSub (Value (_builder), _tmp2.value ());
				}
				if (!m_value)
					return std::nullopt;
				_builder.CreateStore (_v.Value (_builder), m_value);
				return *this;
			} else if (_op == "~") {
				if (_typestr != "bool") {
					LOG_ERROR (_t, "��bool���������޷�ȡ��");
					return std::nullopt;
				}
				std::optional<llvm::Value *> _tmp2 = _value_builder->Build (_typestr, "true", _t);
				if (!_tmp2.has_value ())
					return std::nullopt;
				return _builder.CreateSub (_tmp2.value (), _tmp);
			}
		}

		LOG_ERROR (_t, fmt::format ("�ݲ�֧�ֵ������ {}", _op));
		return std::nullopt;
	}
	AstValue DoOper2 (llvm::IRBuilder<> &_builder, std::shared_ptr<ValueBuilder> _value_builder, std::string _op, AstValue &_other, antlr4::Token *_t) {
		if (!_other.IsValid ())
			return std::nullopt;
		AstValue _tmp;
		if (_op.size () == 1) {
			switch (_op [0]) {
			case '+': return _builder.CreateAdd (Value (_builder), _other.Value (_builder));
			case '-': return _builder.CreateSub (Value (_builder), _other.Value (_builder));
			case '*': return _builder.CreateMul (Value (_builder), _other.Value (_builder));
			case '/': return _builder.CreateSDiv (Value (_builder), _other.Value (_builder));
			case '%': return _builder.CreateSRem (Value (_builder), _other.Value (_builder));
			case '|': return _builder.CreateOr (Value (_builder), _other.Value (_builder));
			case '&': return _builder.CreateAnd (Value (_builder), _other.Value (_builder));
			case '^': return _builder.CreateXor (Value (_builder), _other.Value (_builder));
			case '<': return _builder.CreateICmpSLT (Value (_builder), _other.Value (_builder));
			case '>': return _builder.CreateICmpSGT (Value (_builder), _other.Value (_builder));
			case '=':
				_builder.CreateStore (_other.Value (_builder), m_value);
				return *this;
			}
		} else if (_op.size () == 2) {
			if (_op [0] == _op [1]) {
				switch (_op [0]) {
				case '?': return _builder.CreateAdd (Value (_builder), _other.Value (_builder));
				case '*': return _builder.CreateAdd (Value (_builder), _other.Value (_builder));
				case '&': return _builder.CreateAnd (Value (_builder), _other.Value (_builder));
				case '|': return _builder.CreateOr (Value (_builder), _other.Value (_builder));
				case '<': return _builder.CreateShl (Value (_builder), _other.Value (_builder));
				case '>':
					_tmp = _other.DoOper1 (_builder, _value_builder, "-", _t);
					return _builder.CreateShl (Value (_builder), _tmp.Value (_builder));
				case '=':
					return _builder.CreateICmpEQ (Value (_builder), _other.Value (_builder));
				}
			} else if (_op [1] == '=') {
				switch (_op [0]) {
				case '<': return _builder.CreateICmpSLE (Value (_builder), _other.Value (_builder));
				case '>': return _builder.CreateICmpSGE (Value (_builder), _other.Value (_builder));
				case '+':
				case '-':
				case '*':
				case '/':
				case '%':
				case '|':
				case '&':
				case '^':
					_tmp = DoOper2 (_builder, _value_builder, _op.substr (0, 1), _other, _t);
					return DoOper2 (_builder, _value_builder, "=", _tmp, _t);
				case '!': return _builder.CreateICmpNE (Value (_builder), _other.Value (_builder));
				}
			}
		} else if (_op.size () == 3) {
			if (_op == "<<=" || _op == ">>=") {
				_tmp = DoOper2 (_builder, _value_builder, _op.substr (0, 2), _other, _t);
				return DoOper2 (_builder, _value_builder, "=", _tmp, _t);
			}
		}
		return std::nullopt;
	}

private:
	AstObjectType m_type = AstObjectType::None;
	llvm::Value *m_value = nullptr;
	llvm::Function *m_func = nullptr;
	std::string m_value_type = "";
};



#endif //__AST_VALUE_HPP__
