#ifndef __FA_LLVM_GEN_HPP__
#define __FA_LLVM_GEN_HPP__



#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include "CodeVisitor.hpp"
#include "TypeMap.hpp"
#include "ValueBuilder.hpp"
#include "AstValue.h"
#include "FuncContext.hpp"
#include "AstExprOrValue.hpp"
#include "FuncType.h"
#include "AstClass.h"



class FaLLVMGen {
	PublicLevel _public_level (FaParser::PublicLevelContext * _public_raw, PublicLevel _default) {
		if (_public_raw == nullptr)
			return _default;
		PublicLevel _ret = m_visitor.visit (_public_raw).as<PublicLevel> ();
		return _ret == PublicLevel::Unknown ? _default : _ret;
	}

public:
	FaLLVMGen (std::string _module_name, std::string _namespace, std::vector<std::string>& _libs, std::map<std::string, std::shared_ptr<FuncType>>& _global_funcs, AstClasses& _global_classes): m_module_name (_module_name), m_namespace (_namespace), m_libs (_libs), m_global_classes (_global_classes) {
		m_ctx = std::make_shared<llvm::LLVMContext> ();
		m_module = std::make_shared<llvm::Module> (m_module_name, *m_ctx);
		m_type_map = std::make_shared<TypeMap> (&m_visitor, m_ctx, _global_classes, m_namespace);
		m_value_builder = std::make_shared<ValueBuilder> (m_type_map, m_ctx, m_module);
		m_local_funcs = std::make_shared<FuncTypes> (m_ctx, m_type_map, m_module, m_value_builder, m_local_funcs_data);
		m_global_funcs = std::make_shared<FuncTypes> (m_ctx, m_type_map, m_module, m_value_builder, _global_funcs);
		_libs.push_back (std::format ("{}.obj", _module_name));
	}

	bool InitClassVar (std::string _file, std::string _source, std::shared_ptr<antlr4::ANTLRInputStream> _stream, std::shared_ptr<FaLexer> _lexer, std::shared_ptr<antlr4::CommonTokenStream> _cts) {
		m_file = _file;
		m_source = _source;
		m_stream = _stream;
		m_lexer = _lexer;
		m_cts = _cts;
		m_parser = std::make_shared<FaParser> (m_cts.get ());
		FaParser::ImportBlockContext* _imports;
		std::tie (m_uses, _imports, m_classes, m_entry) = m_visitor.visit (m_parser->program ()).as<std::tuple<
			std::vector<std::string>,
			FaParser::ImportBlockContext*,
			std::vector<FaParser::ClassStmtContext*>,
			FaParser::FaMainFuncBlockContext*
			>> ();

		// �����ⲿģ��
		if (_imports) {
			auto [_imports_raw, _libs] = m_visitor.visit (_imports).as<std::tuple<
				std::vector<FaParser::ImportStmtContext*>,
				std::vector<std::string>
				>> ();
			for (std::string _lib : _libs)
				m_libs.push_back (_lib);
			if (!ProcessImports (_imports_raw))
				return false;
		}

		// ������ṹ
		for (auto _class_raw : m_classes) {
			// ���ʼ���
			PublicLevel _pl = _public_level (_class_raw->publicLevel (), PublicLevel::Internal);

			// ��������
			std::string _name = std::format ("{}.{}", m_namespace, _class_raw->Id ()->getText ());
			auto _oclass = m_global_classes.GetClass (_name, "");
			if (_oclass.has_value ()) {
				LOG_ERROR (_class_raw->Id ()->getSymbol (), "�����ظ�����");
				return false;
			}

			// ���������
			auto _get_class_var = [&](FaParser::ClassVarContext *_var_raw) -> std::optional<std::shared_ptr<AstClassVar>> {
				// ���ʼ���
				auto _pl = _public_level (_var_raw->publicLevel (), PublicLevel::Private);

				// �Ƿ�̬
				bool _is_static = !!_var_raw->Static ();

				// ����
				std::string _type = _var_raw->type ()->getText ();

				// ����
				std::string _name = _var_raw->Id ()->getText ();
				auto _var = std::make_shared<AstClassVar> (_var_raw->start, _pl, _is_static, _type, _name);

				// ��ʼֵ�� getter setter
				auto _var_ext_raw = _var_raw->classVarExt ();
				if (_var_ext_raw) {
					// ���Ա������������Ծ�Ϊʵ������
					// ��ֵ
					if (_var_ext_raw->tmpAssignExpr ())
						_var->SetInitValue (_var_ext_raw->tmpAssignExpr ()->expr ());

					// ����� getter setter
					for (auto _ext_func_raw : _var_ext_raw->classVarExtFunc ()) {
						_pl = _public_level (_ext_func_raw->publicLevel (), PublicLevel::Public);
						auto [_suc, _err] = _var->AddVarFunc (_pl, _ext_func_raw->Id ()->getText (), _ext_func_raw->classFuncBody ());
						if (!_suc) {
							LOG_ERROR (_ext_func_raw->start, _err);
							return std::nullopt;
						}
					}
				} else {
					// ��ͨ����
					// ��ֵ
					if (_var_raw->tmpAssignExpr ())
						_var->SetInitValue (_var_raw->tmpAssignExpr ()->expr ());
				}
				return _var;
			};

			// ʶ������
			std::string _class_type = _class_raw->classType ()->getText ();

			// ��Ա�������������˴���������
			if (_class_type == "class") {
				std::shared_ptr<AstClass> _class = m_global_classes.CreateNewClass (_pl, m_module_name, _name);
				// ������
				if (_class_raw->classParent ()) {
					auto [_parents, _parent_ts] = m_visitor.visit (_class_raw->classParent ()).as<std::tuple<
						std::vector<std::string>,
						std::vector<antlr4::Token*>
						>> ();
					for (size_t i = 0; i < _parents.size (); ++i) {
						auto [_ocls, _multi] = FindAstClass (_parents [i]);
						if (_multi) {
							LOG_ERROR (_parent_ts [i], std::format ("�޷�׼ȷʶ��ı�ʶ�� {}", _parents [i]));
							return false;
						}
						if (!_ocls.has_value ()) {
							LOG_ERROR (_parent_ts [i], std::format ("δ����ı�ʶ�� {}", _parents [i]));
							return false;
						}
						auto _cls = _ocls.value ();
						_parents [i] = _cls->m_name;
						auto _clstype = _cls->GetType ();
						if (_clstype == AstClassType::Class || _clstype == AstClassType::Struct) {
							
						}
					}
					_class->AddParents (_parents);
				}

				// ���������ö������
				if (_class_raw->enumAtom ().size () > 0) {
					LOG_ERROR (_class_raw->enumAtom (0)->start, std::format ("{} ���͵Ľṹ���������ö�����ͱ���", _class_type));
					return false;
				}

				// ��Ա����
				for (auto _var_raw : _class_raw->classVar ()) {
					auto _var = _get_class_var (_var_raw);
					if (!_var.has_value ())
						return false;
					_class->AddVar (_var.value ());
				}
			} else {
				LOG_TODO (_class_raw->classType ()->start);
				return false;
			}
		}
		return true;
	}

	bool InitClassFunc () {
		for (auto _class_raw : m_classes) {
			std::string _name = std::format ("{}.{}", m_namespace, _class_raw->Id ()->getText ());
			std::shared_ptr<IAstClass> _class = m_global_classes.GetClass (_name, m_namespace).value ();

			// ��Ա����
			for (auto _func_raw : _class_raw->classFunc ()) {
				// ���ʼ���
				auto _pl = _public_level (_func_raw->publicLevel (), PublicLevel::Private);

				// �Ƿ�̬
				bool _is_static = !!_func_raw->Static ();

				// ����
				std::string _name = _func_raw->classFuncName ()->getText ();
				auto _func = _class->AddFunc (_pl, _is_static, _name);

				// ��������
				_func->SetReturnType (_func_raw->type ());

				// �����б�
				if (!_is_static)
					_func->SetArgumentTypeName (_class->m_name, _func_raw->start, "this");
				//
				std::vector<std::string> _arg_types;
				if (_func_raw->typeVarList ()) {
					for (auto _type_var_raw : _func_raw->typeVarList ()->typeVar ()) {
						std::string _arg_type = _type_var_raw->type ()->getText ();
						_arg_type = GetTypeFullName (_arg_type); // todo �˴����ڻ�û���������е��࣬�������Ͳ�����
						std::string _arg_name = _type_var_raw->Id () ? _type_var_raw->Id ()->getText () : "";
						_func->SetArgumentTypeName (_arg_type, _type_var_raw->type ()->start, _arg_name);
					}
				}

				// ������
				_func->SetFuncBody (_func_raw->classFuncBody ());

				// ��������
				_func->m_name_abi = std::format ("{}.{}", _class->m_name, _func->m_name);
				if (!m_global_funcs->Make (_class->m_name, _func->m_name_abi, _func->m_ret_type, _func->m_ret_type_t, _func->m_arg_types, _func->m_arg_type_ts))
					return false;
			}
		}
		return true;
	}

	bool Compile () {
		// ������
		if (!m_global_classes.EnumClasses (m_module_name, [&] (std::shared_ptr<IAstClass> _cls) -> bool {
			for (auto _cls_func : _cls->m_funcs) {
				std::vector<std::tuple<std::string, std::string>> _args;
				for (size_t i = 0; i < _cls_func->m_arg_names.size (); ++i)
					_args.push_back ({ _cls_func->m_arg_names [i], _cls_func->m_arg_types [i] });
				FuncContext _func_ctx { _cls, m_global_classes, m_global_funcs, m_global_funcs, _cls_func->m_name_abi, _cls_func->m_ret_type, m_namespace, _args };
				//
				auto _expr_raw = _cls_func->m_func->expr ();
				if (_expr_raw) {
					std::optional<AstValue> _val = ExprBuilder (_func_ctx, _expr_raw, _cls_func->m_ret_type);
					if (!_val.has_value ())
						return false;
					if (_func_ctx.GetReturnType () == "void") {
						if (!_func_ctx.Return (_expr_raw->start))
							return false;
					} else {
						if (!_func_ctx.Return (_val.value (), _expr_raw->start))
							return false;
					}
				} else {
					auto _stmt = _cls_func->m_func->stmt ();
					bool _a = false;
					if (!StmtBuilder (_func_ctx, _stmt, _a))
						return false;
					if (!_a) {
						if (_cls_func->m_ret_type != "void") {
							LOG_ERROR (_cls_func->m_ret_type_t, "�����践��ָ����������");
							return false;
						}
						if (!_func_ctx.Return (_cls_func->m_ret_type_t))
							return false;
					}
				}
			}
			return true;
		}))
			return false;

		// ����������
		if (m_entry) {
			auto [_ret_type_raw, _stmts_raw] = m_visitor.visit (m_entry).as<std::tuple<
				FaParser::TypeContext*,
				std::vector<FaParser::StmtContext*>
				>> ();
			std::vector<FaParser::TypeContext*> _arg_type_raws;
			if (!m_local_funcs->Make ("", "@fa_main", _ret_type_raw, _arg_type_raws))
				return false;
			std::string _ret_type = _ret_type_raw->getText ();
			std::vector<std::tuple<std::string, std::string>> _args;
			FuncContext _func_ctx { nullptr, m_global_classes, m_global_funcs, m_local_funcs, "@fa_main", _ret_type, m_namespace, _args };
			bool _a = false;
			if (!StmtBuilder (_func_ctx, _stmts_raw, _a))
				return false;
			if (!_a) {
				auto _t = m_entry->QuotHuaR ()->getSymbol ();
				if (_ret_type != "void") {
					LOG_ERROR (_t, "�������践��ָ����������");
					return false;
				}
				if (!_func_ctx.Return (_t))
					return false;
			}
		}

		llvm::InitializeAllTargetInfos ();
		llvm::InitializeAllTargets ();
		llvm::InitializeAllTargetMCs ();
		llvm::InitializeAllAsmParsers ();
		llvm::InitializeAllAsmPrinters ();
		std::string _target_triple = llvm::sys::getDefaultTargetTriple (), _err = "";
		m_module->setTargetTriple (_target_triple);
		const llvm::Target* _target = llvm::TargetRegistry::lookupTarget (_target_triple, _err);
		if (!_target) {
			LOG_ERROR (nullptr, _err);
			return false;
		}
		std::string _cpu = "";
		std::string _features = "";

		llvm::TargetOptions _opt;
		auto _model = llvm::Optional<llvm::Reloc::Model> ();
		auto _target_machine = _target->createTargetMachine (_target_triple, _cpu, _features, _opt, _model);
		m_module->setDataLayout (_target_machine->createDataLayout ());

		std::error_code _ec;
		llvm::raw_fd_ostream _dest (std::format ("{}.obj", m_module_name), _ec, llvm::sys::fs::F_None);
		if (_ec) {
			LOG_ERROR (nullptr, "�޷�������ļ�");
			return false;
		}
		llvm::legacy::PassManager _pass;
		if (_target_machine->addPassesToEmitFile (_pass, _dest, nullptr, llvm::CGFT_ObjectFile)) {
			LOG_ERROR (nullptr, "�޷���������ļ�");
			return false;
		}
		_pass.run (*m_module);
		_dest.flush ();

		llvm::raw_fd_ostream _dest2 (std::format ("{}.ll", m_module_name), _ec, llvm::sys::fs::F_None);
		m_module->print (_dest2, nullptr);
		_dest2.flush ();

		return true;
	}

private:
	bool ProcessImports (std::vector<FaParser::ImportStmtContext*> _imports_raw) {
		// https://blog.csdn.net/adream307/article/details/83820543
		for (FaParser::ImportStmtContext* _import_func_raw : _imports_raw) {
			auto [_name, _ret_type_raw, _arg_types_raw, _cc_str] = m_visitor.visit (_import_func_raw).as<std::tuple<
				std::string,
				FaParser::TypeContext*,
				std::vector<FaParser::TypeContext*>,
				std::string
				>> ();
			std::string _func_name = std::format ("::{}", _name);
			if (!m_local_funcs->Contains (_func_name)) {
				llvm::CallingConv::ID _cc = llvm::CallingConv::C;
				if (_cc_str == "__cdecl") {
					_cc = llvm::CallingConv::C;
				} else if (_cc_str == "__stdcall") {
					_cc = llvm::CallingConv::X86_StdCall;
				} else if (_cc_str == "__fastcall") {
					_cc = llvm::CallingConv::X86_FastCall;
				}
				if (!m_local_funcs->MakeExtern (_func_name, _ret_type_raw, _arg_types_raw, _cc))
					return false;
			}
		}
		return true;
	}

	bool StmtBuilder (FuncContext& _func_ctx, std::vector<FaParser::StmtContext*>& _stmts_raw, bool& _all_path_return) {
		_all_path_return = false;
		for (FaParser::StmtContext* _stmt_raw : _stmts_raw) {
			if (_stmt_raw->normalStmt ()) {
				FaParser::NormalStmtContext* _normal_stmt_raw = _stmt_raw->normalStmt ();
				if (_normal_stmt_raw->Return () || _normal_stmt_raw->expr ()) {
					if (_normal_stmt_raw->expr ()) {
						std::string _exp_type = _normal_stmt_raw->Return () ? _func_ctx.GetReturnType () : "";
						std::optional<AstValue> _oval = ExprBuilder (_func_ctx, _normal_stmt_raw->expr (), _exp_type);
						if (!_oval.has_value ())
							return false;
						AstValue _val = _oval.value ();
						if (_normal_stmt_raw->Return ()) {
							if (!_val.IsValue ()) {
								LOG_ERROR (_normal_stmt_raw->start, "�޷����ر��ʽ���");
								return false;
							}
							if (!_func_ctx.Return (_val, _normal_stmt_raw->start))
								return false;
							_all_path_return = true;
						}
					} else {
						if (_normal_stmt_raw->Return ()) {
							if (!_func_ctx.Return (_normal_stmt_raw->start))
								return false;
							_all_path_return = true;
						}
					}
				} else if (_normal_stmt_raw->Break ()) {
					LOG_TODO (_normal_stmt_raw->start);
					return false;
				} else if (_normal_stmt_raw->Continue ()) {
					LOG_TODO (_normal_stmt_raw->start);
					return false;
				} else {
					LOG_ERROR (_stmt_raw->start, "δ֪�ı��ʽ");
					return false;
				}
			} else if (_stmt_raw->ifStmt ()) {
				std::vector<FaParser::ExprContext*> _conds;
				std::vector<std::vector<FaParser::StmtContext*>> _bodys;
				std::tie (_conds, _bodys) = m_visitor.visit (_stmt_raw->ifStmt ()).as<std::tuple<
					std::vector<FaParser::ExprContext*>,
					std::vector<std::vector<FaParser::StmtContext*>>
					>> ();
				bool _a = false;
				if (!IfStmtBuilder (_func_ctx, _conds, _bodys, _a))
					return false;
				_all_path_return |= _a;
			} else if (_stmt_raw->defVarStmt ()) {
				auto _def_var_stmt_raw = _stmt_raw->defVarStmt ();
				std::string _exp_type = _def_var_stmt_raw->type ()->getText ();
				if (_exp_type == "var")
					_exp_type = "";
				std::optional<AstValue> _oval = ExprBuilder (_func_ctx, _def_var_stmt_raw->expr (), _exp_type);
				if (!_oval.has_value ())
					return false;
				AstValue _val = _oval.value ();
				if (_exp_type == "")
					_exp_type = _val.GetType ();
				std::string _var_name = _def_var_stmt_raw->Id ()->getText ();
				if (_val.GetTmpVarFlag ()) {
					if (!_func_ctx.BindTempVariable (_def_var_stmt_raw->start, _val, _var_name))
						return false;
					_val.SetTmpVarFlag (false);
				} else {
					std::optional<AstValue> _ovar = _func_ctx.DefineVariable (_exp_type, _def_var_stmt_raw->type ()->start, _var_name);
					if (!_ovar.has_value ())
						return false;
					AstValue _var = _ovar.value ();
					auto _oval = _func_ctx.DoOper2 (_var, "=", _val, _def_var_stmt_raw->Assign ()->getSymbol ());
					if (!_oval.has_value ())
						return false;
				}
			} else if (_stmt_raw->whileStmt ()) {
				auto _oev_cond = _expr_parse (_func_ctx, _stmt_raw->whileStmt ()->expr (), "bool");
				if (!_oev_cond.has_value ())
					return false;
				auto _stmts_raw = _stmt_raw->whileStmt ()->stmt ();
				bool _a = false;
				_func_ctx.While (
					_oev_cond.value (),
					[&] () { return StmtBuilder (_func_ctx, _stmts_raw, _a); },
					[&] (_AST_ExprOrValue _ast_ev) { return _generate_code (_func_ctx, _ast_ev); }
				);
			} else if (_stmt_raw->numIterStmt ()) {
				LOG_ERROR (_stmt_raw->start, "δ֪�ı��ʽ");
				return false;
			} else {
				LOG_ERROR (_stmt_raw->start, "δ֪�ı��ʽ");
				return false;
			}
		}
		return true;
	}

	bool IfStmtBuilder (FuncContext& _func_ctx, std::vector<FaParser::ExprContext*>& _conds_raw, std::vector<std::vector<FaParser::StmtContext*>>& _bodys_raw, bool& _all_path_return) {
		if (_conds_raw.size () == 0)
			return StmtBuilder (_func_ctx, _bodys_raw [0], _all_path_return);
		//
		std::optional<AstValue> _ocond = ExprBuilder (_func_ctx, _conds_raw [0], "bool");
		if (!_ocond.has_value ())
			return false;
		_conds_raw.erase (_conds_raw.begin ());
		bool _a = false, _b = false;
		bool _ret = _func_ctx.IfElse (_ocond.value (), [&] () {
			if (!StmtBuilder (_func_ctx, _bodys_raw [0], _a))
				return false;
			_bodys_raw.erase (_bodys_raw.begin ());
			return true;
		}, [&] () {
			return IfStmtBuilder (_func_ctx, _conds_raw, _bodys_raw, _b);
		});
		_all_path_return = _a && _b;
		return _ret;
	}

	std::optional<AstValue> ExprBuilder (FuncContext& _func_ctx, FaParser::ExprContext* _expr_raw, std::string _expect_type) {
		// �������������﷨��
		std::optional<_AST_ExprOrValue> _oev = _expr_parse (_func_ctx, _expr_raw, _expect_type);
		if (!_oev.has_value ())
			return std::nullopt;
		// ���ɴ���
		return _generate_code (_func_ctx, _oev.value ());
	}

	std::optional<_AST_ExprOrValue> _expr_parse (FuncContext& _func_ctx, FaParser::ExprContext* _expr_raw, std::string _expect_type) {
		// �����������
		std::function<std::optional<_AST_ExprOrValue> (FaParser::ExprContext*, std::string)> _parse_expr;
		std::function<std::optional<_AST_ExprOrValue> (FaParser::MiddleExprContext*, std::string)> _parse_middle_expr;
		std::function<std::optional<_AST_ExprOrValue> (std::vector<FaParser::StrongExprContext*>& _expr_raws, std::vector<FaParser::AllOp2Context*>& _op_raws, std::vector<size_t>& _op_levels, std::string)> _parse_middle_expr2;
		std::function<std::optional<_AST_ExprOrValue> (FaParser::StrongExprContext*, std::string)> _parse_strong_expr;
		std::function<std::optional<_AST_ExprOrValue> (FaParser::StrongExprBaseContext*, std::string)> _parse_strong_expr_base;
		std::function<std::optional<_AST_ExprOrValue> (FaParser::IfExprContext*, std::string)> _parse_if_expr;
		_parse_expr = [&] (FaParser::ExprContext* _expr_raw, std::string _exp_type) -> std::optional<_AST_ExprOrValue> {
			auto _exprs = _expr_raw->middleExpr ();
			auto _ops = _expr_raw->allAssign ();
			if (_exprs.size () == 1) {
				return _parse_middle_expr (_exprs [0], _exp_type);
			} else {
				if (_exp_type != "" && _exp_type [0] == '$') {
					LOG_ERROR (_ops [0]->start, "����ֵ����ı��ʽ�޷��ٴθ�ֵ");
					return std::nullopt;
				}
				auto _ret = std::make_shared<_AST_Op2ExprTreeCtx> ();
				auto _oval = _parse_middle_expr (_exprs [0], "$");
				if (!_oval.has_value ())
					return std::nullopt;
				_ret->m_left = _oval.value ();
				_ret->m_op = _AST_Oper2Ctx { _ops [0] };
				std::string _cur_type = _ret->m_left.GetExpectType ();
				if (!TypeMap::CanImplicitConvTo (_cur_type, _exp_type))
					return std::nullopt;
				_ret->m_expect_type = _exp_type;
				_exp_type = _cur_type.substr (1);
				auto _cur = _ret;
				for (size_t i = 1; i < _ops.size (); ++i) {
					auto _cur_tmp = std::make_shared<_AST_Op2ExprTreeCtx> ();
					_cur->m_right = _cur_tmp;
					_cur = _cur_tmp;
					_oval = _parse_middle_expr (_exprs [i], "$");
					if (!_oval.has_value ())
						return std::nullopt;
					_cur->m_left = _oval.value ();
					_cur->m_op = _AST_Oper2Ctx { _ops [i] };
					_cur_type = _cur->m_left.GetExpectType ();
					if (!TypeMap::CanImplicitConvTo (_cur_type, _exp_type))
						return std::nullopt;
					_cur->m_expect_type = _exp_type;
					_exp_type = _cur_type.substr (1);
				}
				_oval = _parse_middle_expr (_exprs [_exprs.size () - 1], _exp_type);
				if (!_oval.has_value ())
					return std::nullopt;
				_cur->m_right = _oval.value ();
				return _ret;
			}
		};
		_parse_middle_expr = [&] (FaParser::MiddleExprContext* _expr_raw, std::string _exp_type) -> std::optional<_AST_ExprOrValue> {
			auto _exprs = _expr_raw->strongExpr ();
			auto _ops = _expr_raw->allOp2 ();
			std::vector<size_t> _op_levels;
			for (auto _op : _ops) {
				std::string _op_str = _op->getText ();
				size_t _level = OperAST::GetOpLevel (_op_str);
				if (_level == std::string::npos) {
					LOG_TODO (_op->start);
					return std::nullopt;
				}
				_op_levels.push_back (_level);
			}
			return _parse_middle_expr2 (std::ref (_exprs), std::ref (_ops), std::ref (_op_levels), _exp_type);
		};
		_parse_middle_expr2 = [&] (std::vector<FaParser::StrongExprContext*>& _expr_raws, std::vector<FaParser::AllOp2Context*>& _op_raws, std::vector<size_t>& _op_levels, std::string _exp_type) -> std::optional<_AST_ExprOrValue> {
			if (_expr_raws.size () == 1)
				return _parse_strong_expr (_expr_raws [0], _exp_type);
			size_t _pos = 0, _pos_level = _op_levels [0];
			for (size_t i = 1; i < _op_levels.size (); ++i) {
				if (_op_levels [i] > _pos_level) {
					_pos = i;
					_pos_level = _op_levels [i];
				}
			}

			// �������ȼ����Ϊ����
			auto _ptr = std::make_shared<_AST_Op2ExprTreeCtx> ();
			_ptr->m_op = _AST_Oper2Ctx { _op_raws [_pos] };
			std::vector<FaParser::StrongExprContext*> _expr_raws_L, _expr_raws_R;
			std::vector<FaParser::AllOp2Context*> _op_raws_L, _op_raws_R;
			std::vector<size_t> _op_levels_L, _op_levels_R;
			_expr_raws_L.assign (_expr_raws.begin (), _expr_raws.begin () + _pos + 1);
			if (_expr_raws_L.size () > 1) {
				_op_raws_L.assign (_op_raws.begin (), _op_raws.begin () + _pos);
				_op_levels_L.assign (_op_levels.begin (), _op_levels.begin () + _pos);
			}
			_expr_raws_R.assign (_expr_raws.begin () + _pos + 1, _expr_raws.end ());
			if (_expr_raws_R.size () > 1) {
				_op_raws_R.assign (_op_raws.begin () + _pos + 1, _op_raws.end ());
				_op_levels_R.assign (_op_levels.begin () + _pos + 1, _op_levels.end ());
			}

			// ����������������
			std::string _exp_type_L = "", _exp_type_R = "";
			if (_ptr->m_op.m_type == _Op2Type::Assign) {
				LOG_TODO (_ptr->m_op.m_t);
				return std::nullopt;
			} else if (_ptr->m_op.m_type == _Op2Type::NoChange) {
				if (_exp_type != "") {
					_exp_type_L = _exp_type_R = _exp_type;
				}
			} else if (_ptr->m_op.m_type == _Op2Type::Compare) {
				if (_exp_type != "" && _exp_type != "bool") {
					LOG_ERROR (_ptr->m_op.m_t, std::format ("�Ƚ������޷����� {} ���͵�ֵ", _exp_type));
					return std::nullopt;
				}
			} else {
				if (_ptr->m_op.m_op != "??") {
					LOG_TODO (_ptr->m_op.m_t);
					return std::nullopt;
				}
				if (_exp_type != "") {
					_exp_type_R = _exp_type;
					_exp_type_L = _exp_type [_exp_type.size () - 1] == '?' ? _exp_type : std::format ("{}?", _exp_type);
				}
			}

			// ������
			auto _oval = _parse_middle_expr2 (std::ref (_expr_raws_L), std::ref (_op_raws_L), std::ref (_op_levels_L), _exp_type_L);
			if (!_oval.has_value ())
				return std::nullopt;
			_ptr->m_left = _oval.value ();
			_oval = _parse_middle_expr2 (std::ref (_expr_raws_R), std::ref (_op_raws_R), std::ref (_op_levels_R), _exp_type_R);
			if (!_oval.has_value ())
				return std::nullopt;
			_ptr->m_right = _oval.value ();

			// ȷ�����������
			_exp_type_L = _ptr->m_left.GetExpectType ();
			_exp_type_R = _ptr->m_right.GetExpectType ();
			if (_ptr->m_op.m_type == _Op2Type::Assign) {
				LOG_TODO (_ptr->m_op.m_t);
				return std::nullopt;
			} else if (_ptr->m_op.m_type == _Op2Type::NoChange || _ptr->m_op.m_type == _Op2Type::Compare) {
				if ((_ptr->m_op.m_type == _Op2Type::NoChange && _exp_type == "") || _ptr->m_op.m_type == _Op2Type::Compare) {
					if (_exp_type_L == _exp_type_R) {
						_exp_type = _exp_type_L;
					} else {
						// Ԥ�����Ͳ�ͬ����ô��ȡ��������
						auto _exp_otype = TypeMap::GetCompatibleType (_ptr->m_op.m_t, { _exp_type_L, _exp_type_R });
						if (!_exp_otype.has_value ())
							return std::nullopt;
						_exp_type = _exp_otype.value ();
						if (_exp_type_L != _exp_type) {
							_oval = _parse_middle_expr2 (std::ref (_expr_raws_L), std::ref (_op_raws_L), std::ref (_op_levels_L), _exp_type_L);
							if (!_oval.has_value ())
								return std::nullopt;
							_ptr->m_left = _oval.value ();
						}
						if (_exp_type_R != _exp_type) {
							_oval = _parse_middle_expr2 (std::ref (_expr_raws_R), std::ref (_op_raws_R), std::ref (_op_levels_R), _exp_type_R);
							if (!_oval.has_value ())
								return std::nullopt;
							_ptr->m_right = _oval.value ();
						}
					}
				}
			} else {
				if (_ptr->m_op.m_op == "??") {
					if (_exp_type_L [_exp_type_L.size () - 1] != '?') {
						LOG_ERROR (_ptr->m_op.m_t, "���ɿ������޷�ʹ�� ?? ����");
						return std::nullopt;
					}
					if (_exp_type_R [_exp_type_R.size () - 1] == '?' && _exp_type_L == _exp_type_R) {
						_exp_type = _exp_type_R;
					} else if (_exp_type_L == std::format ("{}?", _exp_type_R)) {
						_exp_type = _exp_type_R;
					} else {
						LOG_ERROR (_ptr->m_op.m_t, "�޷�ʹ�� ?? ����");
						return std::nullopt;
					}
				} else {
					LOG_TODO (_ptr->m_op.m_t);
					return std::nullopt;
				}
			}
			_ptr->m_expect_type = _exp_type;
			return _ptr;
		};
		_parse_strong_expr = [&] (FaParser::StrongExprContext* _expr_raw, std::string _exp_type) -> std::optional<_AST_ExprOrValue> {
			if (_exp_type != "" && _expr_raw->strongExprSuffix ().size () > 0) {
				auto _oeov = _parse_strong_expr (_expr_raw, "");
				if (!_oeov.has_value ())
					return std::nullopt;
				auto _oev = _oeov.value ();
				_oev.SetExpectType (_exp_type);
				return _oev;
			}
			if (_exp_type != "" && _exp_type != "$") {
				// �⵽�ڲ���������
				// prefix 0->x
				// suffix x->0
				_AST_ExprOrValue _ret, _cur;
				bool _init = true;
				std::function<void (_AST_ExprOrValue)> _update_next = [&] (_AST_ExprOrValue _ev) {
					if (_init) {
						_init = false;
						_ret = _cur = _ev;
					} else {
						if (_cur.m_op1_expr) {
							_cur.m_op1_expr->m_left = _ev;
							_cur = _cur.m_op1_expr->m_left;
						} else if (_cur.m_op2_expr) {
							_cur.m_op2_expr->m_left = _ev;
							_cur = _cur.m_op2_expr->m_left;
						} else if (_cur.m_opN_expr) {
							_cur.m_opN_expr->m_left = _ev;
							_cur = _cur.m_opN_expr->m_left;
						}
					}
				};
				//
				for (auto _prefix_raw : _expr_raw->strongExprPrefix ()) {
					auto _ptr = std::make_shared<_AST_Op1ExprTreeCtx> ();
					_ptr->m_op = _AST_Oper1Ctx { _prefix_raw };
					_ptr->m_expect_type = _exp_type;
					_update_next (_ptr);
				}
				//
				auto _suffix_raws = _expr_raw->strongExprSuffix ();
				for (auto _suffix_praw = _suffix_raws.rbegin (); _suffix_praw != _suffix_raws.rend (); ++_suffix_praw) {
					auto _suffix_raw = *_suffix_praw;
					if (_suffix_raw->AddAddOp () || _suffix_raw->SubSubOp ()) {
						auto _ptr = std::make_shared<_AST_Op1ExprTreeCtx> ();
						_ptr->m_op = _AST_Oper1Ctx { _suffix_raw };
						_ptr->m_expect_type = _exp_type;
						if (_ptr->m_expect_type [0] != '$') {
							LOG_ERROR (_suffix_raw->start, "���󲻿ɱ���ֵ");
							return std::nullopt;
						}
						_update_next (_ptr);
					//} else if (_suffix_raw->QuotYuanL () || _suffix_raw->QuotFangL ()) {
					//	auto _ptr = std::make_shared<_AST_OpNExprTreeCtx> ();
					//	_ptr->m_op = _AST_Oper2Ctx { _suffix_raw };
					//	auto _ofunc = m_local_funcs->GetFunc (_exp_type);
					//	if (!_ofunc.has_value ()) {
					//		_ofunc = m_global_funcs->GetFunc (_exp_type);
					//		if (!_ofunc.has_value ()) {
					//			LOG_ERROR (_suffix_raw->start, std::format ("δ����ķ��� {}", _exp_type));
					//			return std::nullopt;
					//		}
					//	}
					//	auto _func = _ofunc.value ();
					//	auto _expr_opt_raws = _suffix_raw->exprOpt ();
					//	if (_expr_opt_raws.size () == 1 && (!_expr_opt_raws [0]->expr ()))
					//		_expr_opt_raws.clear ();
					//	if (_expr_opt_raws.size () != _func->m_arg_types.size ()) {
					//		LOG_ERROR (_suffix_raw->start, "����������ƥ��");
					//		return std::nullopt;
					//	}
					//	for (size_t i = 0; i < _func->m_arg_types.size (); ++i) {
					//		if (_expr_opt_raws [i]->expr ()) {
					//			auto _right_oval = _parse_expr (_expr_opt_raws [i]->expr (), _func->m_arg_types [i]);
					//			if (!_right_oval.has_value ())
					//				return std::nullopt;
					//			_ptr->m_rights.push_back (_right_oval.value ());
					//		} else {
					//			//_ptr->_rights.push_back (_AST_ExprOrValue { std::make_shared<_AST_ValueCtx> (std::nullopt, _expr_opt_raws [i]->start, "?") });
					//			LOG_TODO (_expr_opt_raws [i]->start);
					//			return std::nullopt;
					//		}
					//	}
					//	_exp_type = _ptr->m_expect_type = _func->m_ret_type;
					//	_update_next (_ptr);
					//} else if (_suffix_raw->PointOp ()) {
					//	auto _ptr = std::make_shared<_AST_Op2ExprTreeCtx> ();
					//	_ptr->m_op = _AST_Oper2Ctx { _suffix_raw };
					//	_ptr->m_right = _AST_ExprOrValue { std::make_shared<_AST_ValueCtx> (AstValue { _suffix_raw->Id ()->getText () }, _suffix_raw->Id ()->getSymbol (), "[member]") };
					//	if (!_ptr->CalcExpectType ()) {
					//		LOG_ERROR (_suffix_raw->start, "���󲻴���Ŀ���Ա");
					//		return std::nullopt;
					//	}
					//	_update_next (_ptr);
					} else {
						LOG_TODO (_expr_raw->start);
						return std::nullopt;
					}
				}
				auto _oval = _parse_strong_expr_base (_expr_raw->strongExprBase (), _exp_type);
				if (!_oval.has_value ())
					return std::nullopt;
				_update_next (_oval.value ());
				return _ret;
			} else {
				// �ڵ������������
				// suffix 0->x
				// prefix x->0
				auto _oval = _parse_strong_expr_base (_expr_raw->strongExprBase (), "");
				if (!_oval.has_value ())
					return std::nullopt;
				_AST_ExprOrValue _val = _oval.value ();
				for (auto _suffix_raw : _expr_raw->strongExprSuffix ()) {
					if (_suffix_raw->AddAddOp () || _suffix_raw->SubSubOp ()) {
						auto _ptr = std::make_shared<_AST_Op1ExprTreeCtx> ();
						_ptr->m_op = _AST_Oper1Ctx { _suffix_raw };
						_ptr->m_left = _val;
						_ptr->m_expect_type = _val.GetExpectType ();
						if (_ptr->m_expect_type [0] != '$') {
							LOG_ERROR (_suffix_raw->start, "���󲻿ɱ���ֵ");
							return std::nullopt;
						}
						_val = _ptr;
					} else if (_suffix_raw->QuotYuanL () || _suffix_raw->QuotFangL ()) {
						auto _ptr = std::make_shared<_AST_OpNExprTreeCtx> ();
						_ptr->m_left = _val;
						_ptr->m_op = _AST_Oper2Ctx { _suffix_raw };
						if (_suffix_raw->QuotYuanL ()) {
							auto _func_name_str = _val.GetFuncName ();
							auto _ofunc = m_local_funcs->GetFunc (_func_name_str);
							if (!_ofunc.has_value ()) {
								_ofunc = m_global_funcs->GetFunc (_func_name_str);
								if (!_ofunc.has_value ()) {
									LOG_ERROR (_suffix_raw->start, std::format ("δ����ķ��� {}", _func_name_str));
									return std::nullopt;
								}
							}
							auto _func = _ofunc.value ();
							auto _expr_opt_raws = _suffix_raw->exprOpt ();

							// �����ε���ʽ����Ĭ��ƥ����һ���յĲ������˴��Ƴ�
							if (_expr_opt_raws.size () == 1 && (!_expr_opt_raws [0]->expr ()))
								_expr_opt_raws.clear ();

							// TODO У�鷽��Ϊ�Ǿ�̬������������У��ò��Ҳû����
							bool _this_call = _val.m_op2_expr && _val.m_op2_expr->m_right.GetExpectType () == "[member]" && _expr_opt_raws.size () == _func->m_arg_types.size () - 1;
							if (_expr_opt_raws.size () != _func->m_arg_types.size () && !_this_call) {
								LOG_ERROR (_suffix_raw->start, "����������ƥ��");
								return std::nullopt;
							}

							if (_this_call)
								_ptr->m_rights.push_back (_AST_ExprOrValue { _val.m_op2_expr->m_left });
							for (size_t i = 0; i < _expr_opt_raws.size (); ++i) {
								if (_expr_opt_raws [i]->expr ()) {
									auto _right_oval = _parse_expr (_expr_opt_raws [i]->expr (), _func->m_arg_types [i]);
									if (!_right_oval.has_value ())
										return std::nullopt;
									_ptr->m_rights.push_back (_right_oval.value ());
								} else {
									//_ptr->_rights.push_back (_AST_ExprOrValue { std::make_shared<_AST_ValueCtx> (std::nullopt, _expr_opt_raws [i]->start, "?") });
									LOG_TODO (_expr_opt_raws [i]->start);
									return std::nullopt;
								}
								_ptr->m_expect_type = _func->m_ret_type;
							}
						} else {
							// ��������ʽ
							auto _expr_opt_raws = _suffix_raw->exprOpt ();
							if (_expr_opt_raws.size () != 1) {
								LOG_ERROR (_suffix_raw->start, "�±����������ƥ��");
								return std::nullopt;
							}
							if (_expr_opt_raws [0]->expr ()) {
								auto _right_oval = _parse_expr (_expr_opt_raws [0]->expr (), "int32");
								if (!_right_oval.has_value ())
									return std::nullopt;
								_ptr->m_rights.push_back (_right_oval.value ());
							} else {
								//_ptr->_rights.push_back (_AST_ExprOrValue { std::make_shared<_AST_ValueCtx> (std::nullopt, _expr_opt_raws [i]->start, "?") });
								LOG_TODO (_expr_opt_raws [0]->start);
								return std::nullopt;
							}
							//
							_ptr->m_expect_type = _val.GetExpectType ();
							_ptr->m_expect_type = _ptr->m_expect_type.substr (0, _ptr->m_expect_type.size () - 2);
						}
						_val = _ptr;
					} else if (_suffix_raw->PointOp ()) {
						auto _ptr = std::make_shared<_AST_Op2ExprTreeCtx> ();
						_ptr->m_left = _val;
						_ptr->m_op = _AST_Oper2Ctx { _suffix_raw };
						_ptr->m_right = _AST_ExprOrValue { std::make_shared<_AST_ValueCtx> (AstValue { _suffix_raw->Id ()->getText () }, _suffix_raw->Id ()->getSymbol (), "[member]") };
						if (!_ptr->CalcExpectType (m_global_classes, m_namespace)) {
							LOG_ERROR (_suffix_raw->start, "���󲻴���Ŀ���Ա");
							return std::nullopt;
						}
						_val = _ptr;
					} else {
						LOG_TODO (_expr_raw->start);
						return std::nullopt;
					}
				}
				auto _prefix_raws = _expr_raw->strongExprPrefix ();
				for (auto _prefix_praw = _prefix_raws.rbegin (); _prefix_praw != _prefix_raws.rend (); ++_prefix_praw) {
					auto _ptr = std::make_shared<_AST_Op1ExprTreeCtx> ();
					_ptr->m_op = _AST_Oper1Ctx { *_prefix_praw };
					_ptr->m_left = _val;
					_ptr->m_expect_type = _val.GetExpectType ();
					_val = _ptr;
				}
				if (!TypeMap::CanImplicitConvTo (_val.GetExpectType (), _exp_type))
					return std::nullopt;
				return _val;
			}
		};
		_parse_strong_expr_base = [&] (FaParser::StrongExprBaseContext* _expr_raw, std::string _exp_type) -> std::optional<_AST_ExprOrValue> {
			if (_expr_raw->ColonColon ()) {
				std::string _name = _expr_raw->getText ();
				auto _of = m_local_funcs->GetFunc (_name);
				if (!_of.has_value ()) {
					LOG_ERROR (_expr_raw->start, std::format ("δ����ķ��� {}", _name));
					return std::nullopt;
				}
				auto _f = _of.value ();
				if (!TypeMap::CanImplicitConvTo (_f->m_type, _exp_type))
					return std::nullopt;
				auto _fp = m_local_funcs->GetFuncPtr (_name);
				return std::make_shared<_AST_ValueCtx> (AstValue { _f, _fp }, _expr_raw->start, _exp_type == "" ? _f->m_type : _exp_type);
			} else if (_expr_raw->Id ()) {
				std::optional<AstValue> _oval = FindValueType (_func_ctx, _expr_raw->Id ()->getText (), _expr_raw->Id ()->getSymbol ());
				if (!_oval.has_value ())
					return std::nullopt;
				AstValue _val = _oval.value ();
				if (!TypeMap::CanImplicitConvTo (_val.GetType (), _exp_type))
					return std::nullopt;
				return std::make_shared<_AST_ValueCtx> (_val, _expr_raw->start, _exp_type == "" ? _val.GetType () : _exp_type);
			} else if (_expr_raw->literal ()) {
				AstValue _val { m_value_builder, _expr_raw->literal () };
				if (!TypeMap::CanImplicitConvTo (_val.GetType (), _exp_type))
					return std::nullopt;
				return std::make_shared<_AST_ValueCtx> (_val, _expr_raw->start, _exp_type == "" ? _val.GetType () : _exp_type);
			} else if (_expr_raw->ifExpr ()) {
				return _parse_if_expr (_expr_raw->ifExpr (), _exp_type);
			} else if (_expr_raw->quotExpr ()) {
				return _parse_expr (_expr_raw->quotExpr ()->expr (), _exp_type);
			} else if (_expr_raw->newExpr ()) {
				auto _new_raw = _expr_raw->newExpr ();
				// ����Ԥ����ʵ��
				std::string _cur_type = _new_raw->ids () ? _new_raw->ids ()->getText () : "";
				if (_cur_type != "") {
					if (_exp_type != "") {
						if (_cur_type != _exp_type) {
							if (!TypeMap::CanImplicitConvTo (_cur_type, _exp_type)) {
								LOG_ERROR (_new_raw->start, std::format ("{} �����޷�תΪ {} ����", _cur_type, _exp_type));
								return std::nullopt;
							}
						} else {
							_cur_type = "";
						}
					} else {
						_exp_type = _cur_type;
						_cur_type = "";
					}
				} else {
					if (_exp_type == "")
						_exp_type = "Json";
				}

				// �������
				IAstClass *_cls = nullptr;
				if (_cur_type != "") {
					auto [_ocls, _multi] = FindAstClass (_cur_type);
					if (_multi) {
						LOG_ERROR (_new_raw->start, std::format ("�޷�׼ȷʶ��ı�ʶ�� {}", _cur_type));
						return std::nullopt;
					}
					if (!_ocls.has_value ()) {
						LOG_ERROR (_new_raw->start, std::format ("δ����ı�ʶ�� {}", _cur_type));
						return std::nullopt;
					}
					_cls = _ocls.value ();
					_cur_type = _cls->m_name;
				}
				if (_exp_type != "") {
					auto [_ocls, _multi] = FindAstClass (_exp_type);
					if (_multi) {
						LOG_ERROR (_new_raw->start, std::format ("�޷�׼ȷʶ��ı�ʶ�� {}", _cur_type));
						return std::nullopt;
					}
					if (!_ocls.has_value ()) {
						LOG_ERROR (_new_raw->start, std::format ("δ����ı�ʶ�� {}", _exp_type));
						return std::nullopt;
					}
					if (!_cls)
						_cls = _ocls.value ();
					_exp_type = _ocls.value ()->m_name;
				}

				auto _newval = std::make_shared<_AST_NewCtx> (_expr_raw->start, _cls, _exp_type);
				for (auto _item_raw : _new_raw->newExprItem ()) {
					std::string _var_name = _item_raw->Id ()->getText ();
					auto _oclsvar = _cls->GetMember (_var_name);
					if (!_oclsvar.has_value ()) {
						LOG_ERROR (_new_raw->start, std::format ("�� {} ��δ����ı�ʶ�� {}", _cls->m_name, _var_name));
						return std::nullopt;
					}
					auto _clsvar = _oclsvar.value ();
					std::string _var_exp_type = _clsvar->GetStringType ();
					_AST_ExprOrValue _var_value;
					if (_item_raw->middleExpr ()) {
						//new Obj { _var = 3 };
						auto _oval = _parse_middle_expr (_item_raw->middleExpr (), _var_exp_type);
						if (!_oval.has_value ())
							return std::nullopt;
						_var_value = _oval.value ();
					} else {
						//new Obj { _var };
						auto _oval = _func_ctx.GetVariable (_var_name);
						if (!_oval.has_value ())
							return std::nullopt;
						_var_value = std::make_shared<_AST_ValueCtx> (_oval.value (), _item_raw->start, _var_exp_type);
					}
					if (!_newval->SetInitVar (_var_name, _var_value, _item_raw->start))
						return std::nullopt;
				}
				if (!_newval->CheckVarsAllInit (_new_raw->start, _parse_expr))
					return std::nullopt;

				return _AST_ExprOrValue { _newval };
			} else if (_expr_raw->arrayExpr1 ()) {
				auto _expr_raws = _expr_raw->arrayExpr1 ()->expr ();
				std::string _scapacity = "";
				if (_exp_type != "") {
					if (_exp_type.substr (_exp_type.size () - 3) == "[?]") {
						// TODO �������˴��Ƿ��Ϊ�ڽ�generator���������������ɡ��˴���ʱһ��������
						_exp_type = _exp_type.substr (0, _exp_type.size () - 3);
					} else {
						_exp_type.erase (_exp_type.size () - 1);
						size_t _p = _exp_type.rfind ('[');
						if (_p + 1 < _exp_type.size ())
							_scapacity = _exp_type.substr (_p + 1);
						_exp_type = _exp_type.substr (0, _p);
					}
				}
				auto _oval = _parse_expr (_expr_raws [0], _exp_type);
				if (!_oval.has_value ())
					return std::nullopt;
				std::vector<_AST_ExprOrValue> _vals;
				_vals.push_back (_oval.value ());
				if (_exp_type == "")
					_exp_type = _vals [0].GetExpectType ();
				_oval = _parse_expr (_expr_raws [1], _exp_type);
				if (!_oval.has_value ())
					return std::nullopt;
				_vals.push_back (_oval.value ());
				if (_expr_raws.size () > 2) {
					_oval = _parse_expr (_expr_raws [2], _exp_type);
					if (!_oval.has_value ())
						return std::nullopt;
					_vals.push_back (_oval.value ());
				} else {
					auto _oval2 = AstValue::FromValue (m_value_builder, "1", _exp_type, _expr_raw->start);
					if (!_oval2.has_value ())
						return std::nullopt;
					_vals.push_back (std::make_shared<_AST_ValueCtx> (_oval2.value (), _expr_raw->start, _exp_type));
				}
				return _AST_ExprOrValue { std::make_shared<_AST_Arr1ValueCtx> (_vals, _scapacity, _expr_raw->start, std::format ("{}[]", _exp_type)) };
			} else if (_expr_raw->arrayExpr2 ()) {
				auto _expr_raws = _expr_raw->arrayExpr2 ()->expr ();
				std::string _scapacity = "";
				if (_exp_type != "") {
					if (_exp_type.substr (_exp_type.size () - 3) == "[?]") {
						_exp_type = _exp_type.substr (0, _exp_type.size () - 3);
					} else {
						_exp_type.erase (_exp_type.size () - 1);
						size_t _p = _exp_type.rfind ('[');
						if (_p + 1 < _exp_type.size ())
							_scapacity = _exp_type.substr (_p + 1);
						_exp_type = _exp_type.substr (0, _p);
					}
				}
				std::vector<_AST_ExprOrValue> _vals;
				for (auto _expr_raw : _expr_raws) {
					auto _oexpr = _parse_expr (_expr_raw, _exp_type);
					if (!_oexpr.has_value ())
						return std::nullopt;
					_vals.push_back (_oexpr.value ());
					if (_exp_type == "")
						_exp_type = _vals [0].GetExpectType ();
				}
				return _AST_ExprOrValue { std::make_shared<_AST_Arr2ValueCtx> (_vals, _scapacity, _expr_raw->start, std::format ("{}[]", _exp_type)) };
			}
			LOG_TODO (_expr_raw->start);
			return std::nullopt;
		};
		_parse_if_expr = [&] (FaParser::IfExprContext* _expr_raw, std::string _exp_type)->std::optional<_AST_ExprOrValue> {
			auto _if_expr = std::make_shared<_AST_IfExprTreeCtx> ();
			for (auto _cond_raw : _expr_raw->expr ()) {
				auto _cond_oval = _parse_expr (_cond_raw, "bool");
				if (!_cond_oval.has_value ())
					return std::nullopt;
				_if_expr->m_conds.push_back (_cond_oval.value ());
			}

			// ������������
			std::vector<std::string> _exp_types;
			for (auto _body_raw : _expr_raw->quotStmtExpr ()) {
				// �����⻷���м������ͣ�����Ӱ����ʵ����
				if (!_func_ctx.CreateVirtualEnv<bool> ([&] () {
					// �������룬Ѱ�������¶���������ڼ��������ʽ����ʱ������Ҫ�õ�
					auto _stmts = _body_raw->stmt ();
					bool _a = false;
					if (!StmtBuilder (_func_ctx, _stmts, _a))
						return false;

					// ��ȡ���ʽ����
					//_if_expr->_bodys1_raw.push_back (_body_raw->stmt ());
					auto _stmt_oval = _parse_expr (_body_raw->expr (), _exp_type);
					if (!_stmt_oval.has_value ())
						return false;
					//_if_expr->_bodys2.push_back (_stmt_oval.value ());
					_exp_types.push_back (_stmt_oval.value ().GetExpectType ());
					return true;
				}))
					return std::nullopt;
			}
			auto _real_exp_otype = TypeMap::GetCompatibleType (_expr_raw->start, _exp_types);
			if (!_real_exp_otype.has_value ())
				return std::nullopt;
			_if_expr->m_expect_type = _real_exp_otype.value ();

			// ������ʵ�����������ɴ���
			for (auto _body_raw : _expr_raw->quotStmtExpr ()) {
				_if_expr->m_bodys1_raw.push_back (_body_raw->stmt ());
				_if_expr->m_bodys2.push_back (_body_raw->expr ());
			}
			return _if_expr;
		};

		return _parse_expr (_expr_raw, _expect_type);
	}

	std::optional<AstValue> _generate_code (FuncContext& _func_ctx, _AST_ExprOrValue _ast_ev) {
		// ת������
		auto _trans_type = [] (AstValue _val, std::string _exp_type, antlr4::Token* _t) -> std::optional<AstValue> {
			// TODO
			if (_exp_type == "void")
				return AstValue::FromVoid ();

			std::string _cur_type = _val.GetType ();
			if (_cur_type == _exp_type)
				return _val;
			if (_val.IsFunction () && _val.m_func->m_type == _exp_type)
				return _val;

			LOG_ERROR (_t, std::format ("{} �����޷�תΪ {} ����", _cur_type, _exp_type));
			return std::nullopt;
		};

		std::string _rexp_type = _ast_ev.GetExpectType ();
		if (_ast_ev.m_val) {
			auto _oleft = _trans_type (_ast_ev.m_val->m_val, _rexp_type, _ast_ev.GetToken ());
			return _oleft;
		} else if (_ast_ev.m_arrval1) {
			auto _t = _ast_ev.m_arrval1->m_t;
			std::string _type = _ast_ev.m_arrval2->m_expect_type;
			_type = _type.substr (0, _type.size () - 2);
			auto _oval = _generate_code (_func_ctx, _ast_ev.m_arrval1->m_vals [0]);
			if (!_oval.has_value ())
				return std::nullopt;
			AstValue _start = _oval.value ();
			_oval = _generate_code (_func_ctx, _ast_ev.m_arrval1->m_vals [1]);
			if (!_oval.has_value ())
				return std::nullopt;
			AstValue _stop = _oval.value ();
			_oval = _generate_code (_func_ctx, _ast_ev.m_arrval1->m_vals [2]);
			if (!_oval.has_value ())
				return std::nullopt;
			AstValue _step = _oval.value ();
			//
			AstValue _capacity;
			if (_ast_ev.m_arrval1->m_str_capacity != "") {
				auto _oval2 = AstValue::FromValue (m_value_builder, _ast_ev.m_arrval1->m_str_capacity, _type, _t);
				if (!_oval2.has_value ())
					return std::nullopt;
				_capacity = _oval2.value ();
			} else {
				auto _otmp = _func_ctx.DoOper2 (_stop, "-", _start, _t);
				if (!_otmp.has_value ())
					return std::nullopt;
				_otmp = _func_ctx.DoOper2 (_otmp.value (), "/", _step, _t);
				if (!_otmp.has_value ())
					return std::nullopt;
				_capacity = _otmp.value ();
			}
			auto _oleft = _func_ctx.DefineArrayVariable (_type, _t, _capacity);
			if (!_oleft.has_value ())
				return std::nullopt;
			auto _left = _oleft.value ();
			_left.SetTmpVarFlag (true);
			//
			auto _iter = _func_ctx.DefineVariable (_type, _t).value ();
			_iter.SetTmpVarFlag (true);
			_func_ctx.DoOper2 (_iter, "=", _start, _t);
			auto _cond_expr = std::make_shared<_AST_Op2ExprTreeCtx> ();
			_cond_expr->m_left = std::make_shared<_AST_ValueCtx> (_iter, _t, _type);
			_cond_expr->m_op = _AST_Oper2Ctx {};
			_cond_expr->m_op.m_op = "==";
			_cond_expr->m_op.m_t = _t;
			_cond_expr->m_op.m_type = _Op2Type::Compare;
			_cond_expr->m_right = std::make_shared<_AST_ValueCtx> (_stop, _t, _type);
			_cond_expr->m_expect_type = "bool";
			auto _cond_expr_wrap = _AST_ExprOrValue { _cond_expr };
			//
			auto _idx = _func_ctx.DefineVariable (_type, _t).value ();
			_idx.SetTmpVarFlag (true);
			auto _0 = AstValue::FromValue (m_value_builder, "0", _type, _t).value ();
			_func_ctx.DoOper2 (_idx, "=", _0, _t);
			//
			_func_ctx.While (_cond_expr_wrap, [&] () {
				auto _tmp = _func_ctx.AccessArrayMember (_left, _idx, _t).value ();
				_func_ctx.DoOper2 (_tmp, "=", _iter, _t);
				//
				_tmp = _func_ctx.DoOper2 (_iter, "+", _step, _t).value ();
				_func_ctx.DoOper2 (_iter, "=", _tmp, _t);
				_func_ctx.DoOper1 (_idx, "++", _t);
				return true;
			}, [&] (_AST_ExprOrValue _ast_ev) { return _generate_code (_func_ctx, _ast_ev); });
			auto _arr_size = _left.GetArrSize ();
			_func_ctx.DoOper2 (_arr_size, "=", _idx, _t);
			return _left;
		} else if (_ast_ev.m_arrval2) {
			auto _t = _ast_ev.m_arrval2->m_t;
			std::string _type = _ast_ev.m_arrval2->m_expect_type;
			_type = _type.substr (0, _type.size () - 2);
			//
			AstValue _capacity;
			std::string _str_capacity = _ast_ev.m_arrval2->m_str_capacity;
			if (_str_capacity == "")
				_str_capacity = std::to_string (_ast_ev.m_arrval2->m_vals.size ());
			auto _oval2 = AstValue::FromValue (m_value_builder, _str_capacity, _type, _t);
			if (!_oval2.has_value ())
				return std::nullopt;
			_capacity = _oval2.value ();
			//
			auto _oleft = _func_ctx.DefineArrayVariable (_type, _t, _capacity);
			if (!_oleft.has_value ())
				return std::nullopt;
			auto _left = _oleft.value ();
			_left.SetTmpVarFlag (true);
			//
			auto _idx = _func_ctx.DefineVariable (_type, _t).value ();
			_idx.SetTmpVarFlag (true);
			auto _0 = AstValue::FromValue (m_value_builder, "0", _type, _t).value ();
			_func_ctx.DoOper2 (_idx, "=", _0, _t);
			for (auto& _val_expr : _ast_ev.m_arrval2->m_vals) {
				auto _oval = _generate_code (_func_ctx, _val_expr);
				if (!_oval.has_value ())
					return std::nullopt;

				auto _tmp = _func_ctx.AccessArrayMember (_left, _idx, _t).value ();
				_func_ctx.DoOper2 (_tmp, "=", _oval.value (), _t);
				_func_ctx.DoOper1 (_idx, "++", _t);
			}
			auto _arr_size = _left.GetArrSize ();
			_func_ctx.DoOper2 (_arr_size, "=", _idx, _t);
			return _left;
		} else if (_ast_ev.m_newval) {
			auto _oleft = _func_ctx.DefineVariable (_ast_ev.m_newval->m_cls->m_name, _ast_ev.m_newval->m_t);
			if (!_oleft.has_value ())
				return std::nullopt;
			auto _left = _oleft.value ();
			_left.SetTmpVarFlag (true);
			//
			if (!_func_ctx.InitClass (_left, _ast_ev.m_newval, [&] (_AST_ExprOrValue _ast_ev) { return _generate_code (_func_ctx, _ast_ev); }))
				return std::nullopt;
			return _left;
		} else if (_ast_ev.m_op1_expr) {
			auto _oleft = _generate_code (_func_ctx, _ast_ev.m_op1_expr->m_left);
			if (!_oleft.has_value ())
				return std::nullopt;
			auto _left = _oleft.value ();
			//
			_oleft = _trans_type (_left, _rexp_type, _ast_ev.GetToken ());
			if (!_oleft.has_value ())
				return std::nullopt;
			_left = _oleft.value ();
			//
			auto _val = _func_ctx.DoOper1 (_left, _ast_ev.m_op1_expr->m_op.m_op, _ast_ev.m_op1_expr->m_op.m_t);
			if (!_val.has_value ())
				return std::nullopt;
			return _val.value ();
		} else if (_ast_ev.m_op2_expr) {
			auto [_rexp_type_l, _rexp_type_r] = TypeMap::GetOp2OperatorExpectType (_rexp_type, );

			auto _oleft = _generate_code (_func_ctx, _ast_ev.m_op2_expr->m_left);
			if (!_oleft.has_value ())
				return std::nullopt;
			auto _left = _oleft.value ();
			_oleft = _trans_type (_left, _rexp_type, _ast_ev.GetToken ());
			if (!_oleft.has_value ())
				return std::nullopt;
			_left = _oleft.value ();
			//
			auto _oright = _generate_code (_func_ctx, _ast_ev.m_op2_expr->m_right);
			if (!_oright.has_value ())
				return std::nullopt;
			auto _right = _oright.value ();
			_oright = _trans_type (_right, _rexp_type, _ast_ev.GetToken ());
			if (!_oright.has_value ())
				return std::nullopt;
			_right = _oright.value ();
			//
			auto _oval = _func_ctx.DoOper2 (_left, _ast_ev.m_op2_expr->m_op.m_op, _right, _ast_ev.m_op2_expr->m_op.m_t);
			if (!_oval.has_value ())
				return std::nullopt;
			auto _val = _trans_type (_oval.value (), _rexp_type, _ast_ev.GetToken ());
			return _val;
		} else if (_ast_ev.m_opN_expr) {
			if (_ast_ev.m_opN_expr->m_op.m_op == "()") {
				auto _oobj = _generate_code (_func_ctx, _ast_ev.m_opN_expr->m_left);
				if (!_oobj.has_value ())
					return std::nullopt;
				AstValue _obj = _oobj.value ();
				std::vector<std::string> _arg_types = std::get<1> (_obj.GetFuncType ());
				std::vector<AstValue> _args;
				for (size_t i = 0; i < _arg_types.size (); ++i) {
					auto _oarg = _generate_code (_func_ctx, _ast_ev.m_opN_expr->m_rights [i]);
					if (!_oarg.has_value ())
						return std::nullopt;
					_oarg = _trans_type (_oarg.value (), _arg_types [i], _ast_ev.m_opN_expr->m_rights [i].GetToken ());
					if (!_oarg.has_value ())
						return std::nullopt;
					_args.push_back (_oarg.value ());
				}
				AstValue _val = _func_ctx.FuncInvoke (_obj, _args);
				auto _oval = _trans_type (_val, _rexp_type, _ast_ev.GetToken ());
				if (!_oval.has_value ())
					return std::nullopt;
				return _oval.value ();
			} else if (_ast_ev.m_opN_expr->m_op.m_op == "[]") {
				auto _oobj = _generate_code (_func_ctx, _ast_ev.m_opN_expr->m_left);
				if (!_oobj.has_value ())
					return std::nullopt;
				//
				if (_ast_ev.m_opN_expr->m_rights.size () != 1) {
					LOG_TODO (_ast_ev.m_opN_expr->m_op.m_t);
					return std::nullopt;
				}
				auto _oidx = _generate_code (_func_ctx, _ast_ev.m_opN_expr->m_rights [0]);
				if (!_oidx.has_value ())
					return std::nullopt;
				auto _oval = _func_ctx.AccessArrayMember (_oobj.value (), _oidx.value (), _ast_ev.GetToken ());
				if (!_oval.has_value ())
					return std::nullopt;
				_oval = _trans_type (_oval.value (), _rexp_type, _ast_ev.GetToken ());
				if (!_oval.has_value ())
					return std::nullopt;
				return _oval.value ();
			} else {
				LOG_TODO (_ast_ev.m_opN_expr->m_op.m_t);
				return std::nullopt;
			}
		} else if (_ast_ev.m_if_expr) {
			auto _ovar_temp = _func_ctx.DefineVariable (_rexp_type, _ast_ev.m_if_expr->m_bodys2 [0]->start);
			if (!_ovar_temp.has_value ())
				return std::nullopt;
			auto _var_temp = _ovar_temp.value ();
			_var_temp.SetTmpVarFlag (true);
			//
			std::function<bool ()> _process_first_block = [&] () {
				bool _a = false;
				if (!StmtBuilder (_func_ctx, _ast_ev.m_if_expr->m_bodys1_raw [0], _a))
					return false;
				_ast_ev.m_if_expr->m_bodys1_raw.erase (_ast_ev.m_if_expr->m_bodys1_raw.begin ());
				auto _oblock_ret = ExprBuilder (_func_ctx, _ast_ev.m_if_expr->m_bodys2 [0], _rexp_type);
				if (!_oblock_ret.has_value ())
					return false;
				auto _ret = _func_ctx.DoOper2 (_var_temp, "=", _oblock_ret.value (), _ast_ev.m_if_expr->m_bodys2 [0]->start);
				if (!_ret.has_value ())
					return false;
				_ast_ev.m_if_expr->m_bodys2.erase (_ast_ev.m_if_expr->m_bodys2.begin ());
				return true;
			};
			//
			std::function<bool ()> _generate_ifexpr;
			_generate_ifexpr = [&] () -> bool {
				if (_ast_ev.m_if_expr->m_conds.size () > 0) {
					// if {} else
					auto _ocond = _generate_code (_func_ctx, _ast_ev.m_if_expr->m_conds [0]);
					if (!_ocond.has_value ())
						return false;
					_ast_ev.m_if_expr->m_conds.erase (_ast_ev.m_if_expr->m_conds.begin ());
					auto _cond = _ocond.value ();
					return _func_ctx.IfElse (_cond, [&] () -> bool {
						return _process_first_block ();
					}, [&] () -> bool {
						return _generate_ifexpr ();
					});
				} else {
					// else {}
					return _process_first_block ();
				}
			};
			if (!_generate_ifexpr ())
				return std::nullopt;
			_ovar_temp = _trans_type (_var_temp, _rexp_type, _ast_ev.GetToken ());
			if (!_ovar_temp.has_value ())
				return std::nullopt;
			return _ovar_temp.value ();
		}
		LOG_TODO (nullptr);
		return std::nullopt;
	}

	std::tuple<std::optional<IAstClass*>, bool/*�Ƿ��ظ�*/> FindAstClass (std::string _raw_name) {
		auto _pcls = m_global_classes.GetClass (_raw_name, m_namespace);
		for (std::string _use : m_uses) {
			std::string _raw_name2 = std::format ("{}.{}", _use, _raw_name);
			auto _pcls2 = m_global_classes.GetClass (_raw_name2, m_namespace);
			if (_pcls.has_value () && _pcls2.has_value ()) {
				return std::make_tuple<std::optional<IAstClass*>, bool> (_pcls.value ().get (), true);
			} else if (!_pcls.has_value ()) {
				_pcls = _pcls2;
			}
		}
		return std::make_tuple<std::optional<IAstClass*>, bool> (_pcls.value ().get (), false);
	}

	std::optional<AstValue> FindValueType (FuncContext& _func_ctx, std::string _raw_name, antlr4::Token* _t) {
		//size_t _p = _raw_name.find ('.');
		//if (_p != std::string::npos) {
		//	// ���� . �����
		//	std::string _prefix = _raw_name.substr (0, _p);
		//	std::string _suffix = _raw_name.substr (_p + 1);
		//
		//	// �²�ǰ����Ǳ���
		//	auto _oval = _func_ctx.GetVariable (_prefix);
		//	if (_oval.has_value ()) {
		//		AstValue _val = _oval.value ();
		//		return _func_ctx.AccessMember (_val, _suffix, _t);
		//	}
		//
		//	//// �²�ǰ����ǲ���
		//	//_oval = _func_ctx.GetArgument (_prefix);
		//	//if (_oval.has_value ()) {
		//	//	AstValue _val = _oval.value ();
		//	//	return _func_ctx.AccessMember (_val, _suffix, _t);
		//	//}
		//
		//	//// �²�ǰ��ο�������
		//	//auto [_oct, _multi] = FindAstClass (_prefix);
		//	//if (_multi) {
		//	//	LOG_ERROR (_t, std::format ("�޷�׼ȷʶ��ı�ʶ�� {}", _prefix));
		//	//	return std::nullopt;
		//	//}
		//	//if (_oct.has_value ()) {
		//	//	// ��̬����/��̬������ö��ֵ
		//	//	auto _oitem = _oct.value ()->GetMember (_suffix);
		//	//	if (_oitem.has_value ()) {
		//	//		auto _item = _oitem.value ();
		//	//		if (_item->IsStatic ()) {
		//	//			//return _item->GetAstValue ();
		//	//			if (_item->GetType () == AstClassItemType::Var) {
		//	//				// ȫ�־�̬���ԣ��˴�����ȫ�ֱ���
		//	//			} else if (_item->GetType () == AstClassItemType::Func) {
		//	//				// ��̬����
		//	//				auto _func = dynamic_cast<AstClassFunc*> (_item);
		//	//				auto _f = m_global_funcs->GetFunc (_func->m_name_abi).value ();
		//	//				auto _fp = m_global_funcs->GetFuncPtr (_func->m_name_abi);
		//	//				return AstValue { _f, _fp };
		//	//			}
		//	//		}
		//	//	}
		//	//	//// �²�����Ǿ�̬����
		//	//	//auto _ovar = _ct->GetVar (_suffix);
		//	//	//if (_ovar.has_value ()) {
		//	//	//	auto& _val = _ovar.value ();
		//	//	//	if (_val->m_is_static) {
		//	//	//		// TODO ����ȫ�ֱ������˴�����ȫ�ֱ���
		//	//	//	}
		//	//	//}
		//
		//	//	//// �²�����Ǿ�̬����
		//	//	//auto _ofunc = _ct->GetFunc (_suffix);
		//	//	//if (_ofunc.has_value ()) {
		//	//	//	auto& _func = _ofunc.value ();
		//	//	//	auto _f = m_global_funcs->GetFunc (_func->m_name_abi).value ();
		//	//	//	auto _fp = m_global_funcs->GetFuncPtr (_func->m_name_abi);
		//	//	//	return AstValue { _f, _fp };
		//	//	//}
		//	//}
		//} else {
		//	// ������ . �����

		// �²�����Ǿֲ�����
		auto _oval = _func_ctx.GetVariable (_raw_name);
		if (_oval.has_value ())
			return _oval;

		// �²�����ǲ���
		_oval = _func_ctx.GetArgument (_raw_name);
		if (_oval.has_value ())
			return _oval;

		// �²���������Ա
		if (_func_ctx.m_cls) {
			auto _oitem = _func_ctx.m_cls->GetMember (_raw_name);
			if (_oitem.has_value ()) {
				auto _item = _oitem.value ();
				if (_item->GetType () == AstClassItemType::Var) {
					auto _var = dynamic_cast<AstClassVar*> (_item);
					if (!_var->IsStatic ()) {
						_oval = _func_ctx.GetArgument ("this");
						if (_oval.has_value ()) {
							_oval = _func_ctx.AccessMember (_oval.value (), _raw_name, _t);
							if (_oval.has_value ())
								return _oval;
						}
					}
				} else if (_item->GetType () == AstClassItemType::Func) {
					auto _func = dynamic_cast<AstClassFunc*> (_item);
					return _func->GetAstValue (m_global_funcs);
				}
				LOG_TODO (_t);
				return std::nullopt;
			}
		}

		// �²�����������ռ����
		return AstValue { _raw_name };
	}

	std::string GetTypeFullName (std::string _type) {
		static std::set<std::string> s_basic_types { "cptr", "int8", "int16", "int32", "int64", "int128", "uint8", "uint16", "uint32", "uint64", "uint128", "void", "bool", "float16", "float32", "float64", "float128" };
		if (s_basic_types.contains (_type))
			return _type;

		auto [_oct, _multi] = FindAstClass (_type);
		if (_multi) {
			LOG_ERROR (nullptr, std::format ("�޷�׼ȷʶ��ı�ʶ�� {}", _type));
			return _type;
		}
		if (_oct.has_value ())
			return _oct.value ()->m_name;
		return _type;
	}

	CodeVisitor											m_visitor {};
	std::string											m_module_name;
	std::string											m_namespace;
	std::shared_ptr<llvm::LLVMContext>					m_ctx;
	std::shared_ptr<llvm::Module>						m_module;
	std::shared_ptr<TypeMap>							m_type_map;
	std::shared_ptr<ValueBuilder>						m_value_builder;
	std::map<std::string, std::shared_ptr<FuncType>>	m_local_funcs_data;
	std::shared_ptr<FuncTypes>							m_local_funcs;
	std::shared_ptr<FuncTypes>							m_global_funcs;
	AstClasses&											m_global_classes;

	std::vector<std::string>							m_uses;
	std::vector<FaParser::ClassStmtContext*>			m_classes;
	FaParser::FaMainFuncBlockContext*					m_entry = nullptr;

public:
	std::vector<std::string>& m_libs;
	std::string											m_file, m_source;

private:
	std::shared_ptr<antlr4::ANTLRInputStream>			m_stream;
	std::shared_ptr<FaLexer>							m_lexer;
	std::shared_ptr<antlr4::CommonTokenStream>			m_cts;
	std::shared_ptr<FaParser>							m_parser;
};



#endif //__FA_LLVM_GEN_HPP__
