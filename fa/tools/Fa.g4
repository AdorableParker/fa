// C++�﷨
// https://github.com/antlr/grammars-v4/blob/master/cpp/CPP14Lexer.g4
// https://github.com/antlr/grammars-v4/blob/master/cpp/CPP14Parser.g4

// ANTLR�ĵ�
// https://decaf-lang.github.io/minidecaf-tutorial/
// https://wizardforcel.gitbooks.io/antlr4-short-course/content/

grammar Fa;



//
// keyword
//
AImport:					'@import';
ALib:						'@lib';
Break:						'break';
CC__Cdecl:					'__cdecl';
CC__FastCall:				'__fastcall';
CC__StdCall:				'__stdcall';
Continue:					'continue';
Class:						'class';
Const:						'const';
Else:						'else';
Enum:						'enum';
FaMain:						'FaMain';
For:						'for';
If:							'if';
Interface:					'interface';
Internal:					'internal';
Namespace:					'namespace';
New:						'new';
Public:						'public';
Protected:					'protected';
Private:					'private';
Return:						'return';
Signed:						'signed';
Static:						'static';
Step:						'step';
Unsigned:					'unsigned';
Use:						'use';
While:						'while';



//
// element
//

// ��ֵ����
Assign:						'=';
addAssign:					AddOp Assign;
subAssign:					SubOp Assign;
starAssign:					StarOp Assign;
divAssign:					DivOp Assign;
modAssign:					ModOp Assign;
orAssign:					OrOp Assign;
andAssign:					AndOp Assign;
xorAssign:					XorOp Assign;
qusQusAssign:				qusQusOp Assign;
starStarAssign:				starStarOp Assign;
andAndAssign:				andAndOp Assign;
orOrAssign:					orOrOp Assign;
shiftLAssign:				shiftLOp Assign;
shiftRAssign:				shiftROp Assign;
allAssign:					Assign | qusQusAssign | addAssign | subAssign | starAssign | starStarAssign | divAssign | modAssign | andAssign | orAssign | xorAssign | andAndAssign | orOrAssign | shiftLAssign | shiftRAssign;

// һԪ����
ReverseOp:					'~';
AddAddOp:					'++';
SubSubOp:					'--';

// ��Ԫ����
PointPoint:					'..';
PointOp:					'.';
AddOp:						'+';
SubOp:						'-';
StarOp:						'*';
DivOp:						'/';
ModOp:						'%';
OrOp:						'|';
AndOp:						'&';
XorOp:						'^';
qusQusOp:					Qus Qus;
starStarOp:					StarOp StarOp;
andAndOp:					AndOp AndOp;
orOrOp:						OrOp OrOp;
shiftLOp:					QuotJianL QuotJianL;
shiftROp:					QuotJianR QuotJianR;

// ��Ԫ������
Qus:						'?';
Comma:						',';
ColonColon:					'::';
Colon:						':';
Semi:						';';
Exclam:						'!';

// ����
QuotFangL:					'[';
QuotFangR:					']';
QuotJianL:					'<';
QuotJianR:					'>';
QuotHuaL:					'{';
QuotHuaR:					'}';
QuotYuanL:					'(';
QuotYuanR:					')';

// �Ƚ�   TODO
ltOp:						QuotJianL;
ltEqualOp:					QuotJianL Assign;
gtOp:						QuotJianR;
gtEqualOp:					QuotJianR Assign;
equalOp:					Assign Assign;
notEqualOp:					Exclam Assign;
exprFuncDef:				Assign QuotJianR;


selfOp2:					AddOp | SubOp | StarOp | DivOp | starStarOp | ModOp | AndOp | OrOp | XorOp | andAndOp | orOrOp | shiftLOp | shiftROp;
compareOp2:					ltOp | ltEqualOp | gtOp | gtEqualOp | equalOp | notEqualOp;
changeOp2:					qusQusOp | compareOp2;
allOp2:						selfOp2 | changeOp2;



//
// Literal
//
fragment SimpleEscape:		'\\\'' | '\\"' | '\\\\' | '\\n' | '\\r' | ('\\' ('\r' '\n'? | '\n')) | '\\t';
fragment HexEscape:			'\\x' HEX HEX;
fragment UniEscape:			'\\u' HEX HEX HEX HEX;
fragment Schar:				SimpleEscape | HexEscape | UniEscape | ~["\\\r\n];
//
BoolLiteral:				'true' | 'false';
IntLiteral:					NUM+;
intNum:						SubOp? IntLiteral;
FloatLiteral:				NUM+ PointOp NUM+;
floatNum:					SubOp? FloatLiteral;
String1Literal:				'"' Schar* '"';
literal:					BoolLiteral | intNum | floatNum | String1Literal;

fragment NUM:				[0-9];
fragment HEX:				NUM | [a-fA-F];
fragment ID_BEGIN:			[a-zA-Z_] | ('\\u' HEX HEX HEX HEX);
fragment ID_AFTER:			NUM | [a-zA-Z_] | ('\\u' HEX HEX HEX HEX);
Id:							ID_BEGIN ID_AFTER*;
ids:						Id (PointOp Id)*;



//
// type
//
typeAfter:					(QuotFangL QuotFangR) | AndOp | Qus | (QuotJianL type (Comma type)* QuotJianR) | StarOp;
type:						(Id | (QuotYuanL type (Comma type)+ QuotYuanR)) typeAfter*;
typeNewable:				Id typeAfter*;
//eTypeAfter:					(QuotFangL QuotFangR) | AndOp | StarOp;
//eSign:						Signed | Unsigned;
//eType:						Const? eSign? Id eTypeAfter*;					// int[]&



//
// list
//
typeVar:					type Id?;
typeVarList:				typeVar (Comma typeVar)*;
//eTypeVar:					eType Id?;
//eTypeVarList:				eTypeVar (Comma eTypeVar)*;



//
// if
//
quotStmtPart:				QuotHuaL stmt* QuotHuaR;
quotStmtExpr:				QuotHuaL stmt* expr QuotHuaR;
ifStmt:						If expr quotStmtPart (Else If expr quotStmtPart)* (Else quotStmtPart)?;
ifExpr:						If expr quotStmtExpr (Else If expr quotStmtExpr)* Else quotStmtExpr;



//
// loop
//
whileStmt:					While expr QuotHuaL stmt* QuotHuaR;
numIterStmt:				For Id Colon exprOpt (Colon exprOpt)+ QuotHuaL stmt* QuotHuaR;



//
// expr
//
quotExpr:					QuotYuanL expr QuotYuanR;
exprOpt:					expr?;
newExprItem:				Id (Assign middleExpr)?;
newExpr1:					New ids? QuotHuaL (newExprItem (Comma newExprItem)*)? QuotHuaR;
newExpr2:					New ids? QuotYuanL (expr (Comma expr)*)? QuotYuanR;
newArray:					New ids? QuotFangL middleExpr QuotFangR;
arrayExpr1:					QuotFangL expr PointPoint expr (Step expr)? QuotFangR;
arrayExpr2:					QuotFangL expr (Comma expr)* QuotFangR;
strongExprBase:				(ColonColon? Id) | literal | ifExpr | quotExpr | newExpr1 | newExpr2 | newArray | arrayExpr1 | arrayExpr2;
strongExprPrefix:			SubOp | AddAddOp | SubSubOp | ReverseOp;										// ǰ׺ - ++ -- ~
strongExprSuffix			: AddAddOp | SubSubOp															// ��׺ ++ --
							| (QuotYuanL (expr (Comma expr)*)? QuotYuanR)									//     Write ("")
							| (QuotFangL (exprOpt (Colon exprOpt)*) QuotFangR)								//     list [12]
							| (PointOp Id)																	//     wnd.Name
							;
strongExpr:					strongExprPrefix* strongExprBase strongExprSuffix*;
middleExpr:					strongExpr (allOp2 strongExpr)*;												//      a == 24    a + b - c
expr:						middleExpr (allAssign middleExpr)*;



//
// define variable
//
tmpAssignExpr:				Assign middleExpr Semi;
defVarStmt:					type Id Assign expr Semi;



//
// stmt
//
normalStmt:					((Return? expr?) | Break | Continue) Semi;
stmt:						normalStmt | ifStmt | defVarStmt | whileStmt | numIterStmt;



//
// class
//
publicLevel:				Public | Internal | Protected | Private;
classParent:				Colon ids (Comma ids)*;
classType:					Class | Interface | Enum;
classStmt:					publicLevel? classType Id classParent? QuotHuaL classEnumAtom* (classVar | classFunc)* QuotHuaR;
							// classParent �� class �� struct ר��ʹ��
							// enumItems �� enum ר��ʹ��
							// enum �� classVar ������ʹ��
//
classVarExtFunc:			publicLevel? Id (Semi | classFuncBody);
classVarExt:				QuotHuaL classVarExtFunc+ QuotHuaR tmpAssignExpr?;
classVar:					publicLevel? Static? type Id (Semi | tmpAssignExpr | classVarExt);
//
classFuncName:				Id | (QuotFangL QuotFangR) | allOp2 | allAssign;
classFuncBody:				(exprFuncDef expr Semi) | (QuotHuaL stmt* QuotHuaR);
classFunc:					publicLevel? Static? type classFuncName QuotYuanL typeVarList? QuotYuanR classFuncBody;
//
classEnumAtom:				Id (QuotYuanL type QuotYuanR)? Comma;



//
// file headers
//
useStmt:					Use ids Semi;
callConvention:				CC__Cdecl | CC__FastCall | CC__StdCall;
importStmt:					AImport type callConvention Id QuotYuanL typeVarList QuotYuanR Semi;
libStmt:					ALib String1Literal Semi;
namespaceStmt:				Namespace ids;



//
// fa_entry_main
//
program:					(useStmt | importStmt | libStmt | namespaceStmt)* classStmt*;



//
// skips
//
Comment1:					'/*' .*? '*/' -> channel (HIDDEN);
Comment2:					'//' ~ [\r\n]* -> channel (HIDDEN);
WS:							[ \t\r\n]+ -> channel (HIDDEN);
