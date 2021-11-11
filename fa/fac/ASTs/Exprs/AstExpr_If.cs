﻿using fac.AntlrTools;
using fac.ASTs.Stmts;
using fac.Exceptions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace fac.ASTs.Exprs {
	class AstExpr_If: IAstExpr {
		public IAstExpr Condition { get; set; }
		public List<IAstStmt> IfTrueCodes { get; set; } = null;
		public IAstExpr IfTrue { get; set; }
		public List<IAstStmt> IfFalseCodes { get; set; } = null;
		public IAstExpr IfFalse { get; set; }



		public override void Traversal (int _deep, int _group, Func<IAstExpr, int, int, IAstExpr> _cb) {
			Condition = _cb (Condition, _deep, _group);
			IfTrueCodes.Traversal (_deep + 1, 0, _cb);
			IfTrue = _cb (IfTrue, _deep + 1, 0);
			IfFalseCodes.Traversal (_deep + 1, 1, _cb);
			IfFalse = _cb (IfFalse, _deep + 1, 1);
		}

		public override IAstExpr TraversalCalcType (string _expect_type) {
			if (_expect_type == "")
				_expect_type = GuessType ();
			//
			Condition = Condition.TraversalCalcType ("bool");
			IfTrueCodes.TraversalCalcType ();
			IfTrue = IfTrue.TraversalCalcType (_expect_type);
			IfFalseCodes.TraversalCalcType ();
			IfFalse = IfFalse.TraversalCalcType (_expect_type);
			ExpectType = _expect_type;
			return AstExprTypeCast.Make (this, _expect_type);
		}

		public override string GuessType () {
			return TypeFuncs.GetCompatibleType (IfTrue.GuessType (), IfFalse.GuessType ());
		}

		public override (string, string) GenerateCSharp (int _indent) {
			var (_a, _b) = Condition.GenerateCSharp (_indent);
			var (_c, _d) = IfTrue.GenerateCSharp (_indent + 1);
			var (_e, _f) = IfFalse.GenerateCSharp (_indent + 1);
			if (_a == "" && _c == "" && _e == "" && (IfTrueCodes?.Count ?? 0) == 0 && (IfFalseCodes?.Count ?? 0) == 0) {
				return ("", $"(({_b}) ? ({_d}) : ({_f}))");
			} else {
				StringBuilder _psb = new StringBuilder ();
				var _tmp_var_name = Common.GetTempId ();
				_psb.Append (_a).AppendLine ($"{_indent.Indent ()}{ExpectType} {_tmp_var_name};");
				_psb.AppendLine ($"{_indent.Indent ()}if ({_b}) {{");
				_psb.AppendStmts (IfTrueCodes, _indent + 1);
				_psb.Append (_c).AppendLine ($"{(_indent + 1).Indent ()}{_tmp_var_name} = {_d};");
				_psb.AppendLine ($"{_indent.Indent ()}}} else {{");
				_psb.AppendStmts (IfFalseCodes, _indent + 1);
				_psb.Append (_e).AppendLine ($"{(_indent + 1).Indent ()}{_tmp_var_name} = {_f};");
				_psb.AppendLine ($"{_indent.Indent ()}}}");
				return (_psb.ToString (), _tmp_var_name);
			}
		}

		public override bool AllowAssign () => false;
	}
}
