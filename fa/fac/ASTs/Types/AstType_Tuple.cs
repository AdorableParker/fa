﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace fac.ASTs.Types {
	class AstType_Tuple: IAstType {
		public List<(IAstType _type, string _name)> TupleTypes { get; set; }



		public override (string, string) GenerateCSharp (int _indent) {
			return ("", $"({string.Join (", ", from p in TupleTypes select p._type.GenerateCSharp (_indent).Item2).ToList ()})");
		}
	}
}