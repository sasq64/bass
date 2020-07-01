
const char* grammar6502 = R"(
Root <- (RootEnum / RootExpression / RootDefinition / RootList / Program) _? EOT

RootExpression <- ':x:' Expression
RootDefinition <- ':d:' FnDef
RootList <- ':a:' CallArgs
RootEnum <- ':e:' EnumBlock

Program <- ((_? LineComment? EOL) / (Line (EOL / &EOT)))*
Line <- (Script / AssignLine / OpLine / ~Label) _? LineComment?
AssignLine <- _? ('*' / Assignee) _? '=' (String / Expression)
Assignee <- DotSymbol _?

Script <- '%{' ScriptContents '}%'
ScriptContents <- (!'}%' .)* 

OpLine <- ~Label? _? (MacroCall / Meta / Instruction)

Instruction <- Opcode Arg?
MacroCall <- Call

Meta <- MetaName _  MetaText Block? (_ Symbol _ Block)*
MetaText <- (!(EOL / '}' / '{' / LineComment) .)*
MetaName <- '!' Symbol

FnDef <- FnName '(' FnArgs ')'
FnName <- Symbol
FnArgs <- (FnArg (',' FnArg)*)?
FnArg <- _? Symbol _?

Block <- EOL? _ '{' BlockContents '}'
BlockContents <- SkipProgram

SkipProgram <- (SkipLine EOL)* SkipLine?
SkipLine <- SkipLabel? _? (SkipMeta / SkipInstruction)? _? LineComment?
SkipLabel <- (_? DotSymbol ':') / (DotSymbol (_ / &EOL))
SkipInstruction <- (!(LineComment / EOL / '}') .)*
SkipMeta <- MetaName _  (!(EOL / '}' / '{') .)* Block? (_ Symbol _ Block)*

String <- _ ["] StringContents ["] _
StringContents <- (!["] .)*

EnumBlock <- ((_? LineComment? EOL) / (EnumLine (EOL / &EOT)))*
EnumLine <- _? Symbol (_ '=' (String / Expression))? _? LineComment?

Label <- (_? DotSymbol ':') / (DotSymbol (_ / &EOL))

Arg <- _ (LabelRef / Acc / Imm / IndY / IndX / Ind / AbsX / AbsY / Abs)

IndX <- '(' Expression (TailX0 / TailX1)
TailX0 <- ')' _? ',' _? 'X'i
TailX1 <- ',' _? 'X'i _? ')'

IndY <- '(' Expression (TailY0 / TailY1)
TailY0 <- ')' _? ',' _? 'Y'i
TailY1 <- ',' _? 'Y'i _? ')'

Ind <- '(' Expression ')' &(!Operator)
Acc <- 'a'i &(![a-zA-Z0-9])
Abs <- Expression
AbsX <- Expression ',' _? 'X'i
AbsY <- Expression ',' _? 'Y'i
Imm <- '#' Expression

LabelRef <- '-'+ / '+'+

Opcode <- Symbol

HexNum <- ('$' / '0x') [0-9a-fA-F]+
Octal <- '0o' [0-7]+
Binary <- '0b' [01]+
Decimal <- ([0-9]+ '.')? [0-9]+
Multi <- '0m' [0-3]+
Number <-  HexNum / Binary / Octal / Multi / Decimal

LineComment <- ';' (!EOL .)* &EOL

~_ <- [ \t]*

Symbol <- [_A-Za-z] [_A-Za-z0-9]*
DotSymbol <- LabelChar / ([._A-Za-z] [_A-Za-z0-9]*)

LabelChar <- '-' / '+' / '$'


EOL <- '\r\n' / '\r' / '\n'
EOT <- !.

Expression  <- Atom (Operator Atom)* {
                         precedence
                           L :
                           L &&
                           L ||
                           L |
                           L ^
                           L &
                           L == !=
                           L < > <= >=
                           L <=>
                           L << >>
                           L - +
                           L / * % \
                       }


Atom <- _? (Star / Unary / Number /
        Index / FnCall / Variable / '(' Expression ')') _?

Star <- '*'

Index <- Indexable '[' Expression? (IndexSep Expression?)? ']'

Indexable <- FnCall / Variable

IndexSep <- ':'

Unary <- UnOp Atom
UnOp <- ('!' / '~' / '-' / '<' / '>' )
FnCall <- Call
Call <- CallName '(' CallArgs ')'
CallName <- Symbol
CallArgs <- (CallArg (',' CallArg)*)?
CallArg <- (Symbol '=' !'=')? (String / Expression)
Operator <-  
        '&&' / '||' / '<<' / '>>' / '==' / '!=' /
        '+' / '-' / '*' / '/' / '%' / '\\' /
        '|' / '^' / '&' / '<' / '>'

Variable <- '.'? (Symbol '.')* Symbol

)";
