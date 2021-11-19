﻿using fac.ASTs.Types;
using fac.Exceptions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace fac.ASTs.Exprs.Names {
	class AstExprName_ClassEnum: IAstExprName {
		public IAstClass EnumClass { init; get; }
		public int EnumItemIndex { init; get; }
		public IAstExpr ThisObject { get; set; }
		public IAstExpr AttachExpr { get; set; }



		public override void Traversal (int _deep, int _group, Func<IAstExpr, int, int, IAstExpr> _cb) {
			if (ThisObject != null)
				ThisObject.Traversal (_deep, _group, _cb);
			if (AttachExpr != null)
				AttachExpr.Traversal (_deep, _group, _cb);
		}

		public override IAstExpr TraversalCalcType (IAstType _expect_type) {
			bool _need_attach = EnumClass.ClassEnumItems[EnumItemIndex].AttachType != null;
			if ((AttachExpr != null) != _need_attach) {
				throw new CodeException (Token, $"{EnumClass.ClassEnumItems[EnumItemIndex].Name} 枚举条件{(_need_attach ? "需要附带参数" : "不能附带参数")}，但实际{(_need_attach ? "没有附带参数" : "附带了参数")}");
			}
			if (_need_attach)
				AttachExpr = AttachExpr.TraversalCalcType (EnumClass.ClassEnumItems[EnumItemIndex].AttachType);
			ExpectType = GuessType ();
			return AstExprTypeCast.Make (this, _expect_type);
		}

		public override IAstType GuessType () => new AstType_Class { Token = Token, Class = EnumClass, TemplateTypes = new List<IAstType> () };

		public override (string, string, string) GenerateCSharp (int _indent, Action<string, string> _check_cb) {
			bool _need_attach = EnumClass.ClassEnumItems[EnumItemIndex].AttachType != null;
			if (_need_attach) {
				int _real_var_index = EnumClass.GetRealAttachVarPos (EnumItemIndex);
				var (_a, _b, _c) = AttachExpr.GenerateCSharp (_indent, _check_cb);
				return new (_a, $"new {EnumClass.FullName} {{ @index = {EnumItemIndex}, {EnumClass.ClassVars[_real_var_index].Name} = {_b} }}", _c);
			} else {
				return new ("", $"new {EnumClass.FullName} {{ @index = {EnumItemIndex} }}", "");
			}
			//throw new NotImplementedException ();
		}

		public override bool AllowAssign () => false;
	}
}
