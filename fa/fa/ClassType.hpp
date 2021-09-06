#ifndef __CLASS_TYPE_HPP__
#define __CLASS_TYPE_HPP__



#include <format>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

//#include "AstValue.hpp"

#include <llvm/IR/Type.h>



enum class PublicLevel { Unknown, Public, Internal, Protected, Private };

class ClassType;



// ������� getter setter ����
class ClassVarFunc {
public:
	PublicLevel						m_pv;					// ��������
	std::string						m_name;					// getter setter ����
	FaParser::ClassFuncBodyContext*	m_func;					// ������

	ClassVarFunc (PublicLevel _pv, std::string _name, FaParser::ClassFuncBodyContext* _func)
		: m_pv (_pv), m_name (_name), m_func (_func) {}
};



// �����
class ClassVar {
public:
	PublicLevel						m_pv;					// ��������
	bool							m_is_static;			// �Ƿ�̬
	std::string						m_type;					// ��������
	std::string						m_name;					// ��������
	FaParser::ExprContext*			m_init_value = nullptr;	// ��ʼֵ
	std::vector<ClassVarFunc>		m_var_funcs;			// ���� getter setter ����

	ClassVar (PublicLevel _pv, bool _is_static, std::string _type, std::string _name)
		: m_pv (_pv), m_is_static (_is_static), m_type (_type), m_name (_name) {}

	void SetInitValue (FaParser::ExprContext* _init_value) { m_init_value = _init_value; }

	std::tuple<bool, std::string> AddVarFunc (PublicLevel _pv, std::string _name, FaParser::ClassFuncBodyContext* _func) {
		static std::set<std::string> s_valid_names { "get", "set" };
		if (!s_valid_names.contains (_name))
			return { false, std::format ("�������֧�� {} ����", _name) };

		for (auto& _vfunc : m_var_funcs) {
			if (_vfunc.m_name == _name)
				return { false, std::format ("{} �����Ѵ���", _name) };
		}
		m_var_funcs.push_back (ClassVarFunc { _pv, _name, _func });
		return { true, "" };
	}
};



class ClassFunc {
public:
	ClassFunc () {}
};



class ClassType: public std::enable_shared_from_this<ClassType> {
public:
	PublicLevel								m_level;
	std::string								m_name;
	std::vector<std::string>				m_parents;
	std::vector<std::shared_ptr<ClassVar>>	m_vars;
	llvm::Type								*m_type = nullptr;

	ClassType (PublicLevel _level, std::string _name): m_level (_level), m_name (_name) {}
	void AddParents (std::vector<std::string> &_parents) {
		m_parents.assign (_parents.cbegin (), _parents.cend ());
	}
	std::shared_ptr<ClassVar> AddVar (PublicLevel _pv, bool _is_static, std::string _type, std::string _name) {
		auto _var = std::make_shared<ClassVar> (shared_from_this (), _name, _type, _type);
		m_vars.push_back (_var);
		return _var;
	}

	std::optional<std::shared_ptr<ClassVar>> GetVar (std::string _name) {
		for (auto _var : m_vars) {
			if (_var->m_name == _name)
				return _var;
		}
		return std::nullopt;
	}
};



class ClassTypes {
public:
	std::optional<std::shared_ptr<ClassType>> GetClass (std::string _name) {
		if (m_classes.contains (_name)) {
			return m_classes [_name];
		} else {
			return std::nullopt;
		}
	}
	std::shared_ptr<ClassType> CreateNewClass (PublicLevel _level, std::string _name) {
		auto _cls = std::make_shared<ClassType> (_level, _name);
		m_classes [_name] = _cls;
		return _cls;
	}

private:
	std::map<std::string, std::shared_ptr<ClassType>> m_classes;
};



#endif //__CLASS_TYPE_HPP__
