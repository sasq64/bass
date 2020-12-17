
extern char const * const grammar6502 = R"(

Program <- Statement*

Statement <- Script / MetaBlock / Line

Line <- EndOfLine / NonEmptyLine

NonEmptyLine <- (AssignLine / OpLine / LabelLine / WhiteLine) _ (&'}' / EndOfLine / &EOT)

WhiteLine <- WS

LabelLine <- Label

OpLine <- Label? _ (MacroCall / Instruction)

MetaBlock <- Label? _ (IfBlock / EnumBlock / (MetaDecl (Block / (&'}' / EndOfLine))))

MetaDecl <- CheckDecl / MacroDecl / GenericDecl

GenericDecl <- MetaName _ CallArgs

MacroDecl <- '!macro' WS FnDef

CheckDecl <- '!check' WS DelayedExpression

DelayedExpression <- Expression

IfBlock <- (IfDecl / IfDefDecl / IfNDefDecl) (Block / (&'}' / EndOfLine))
     (_ 'else' (Block / (&'}' / EndOfLine)))?

IfDecl <- '!if' WS Expression
IfDefDecl <- '!ifdef' WS Symbol
IfNDefDecl <- '!ifndef' WS Symbol

EnumBlock <- '!enum' WS OptSymbol EndOfLine? _ '{' EnumLine* _ '}'
EnumLine <- EndOfLine / ( _ Symbol (_ '=' Expression)?  _ (&'}' / EndOfLine) _)

OptSymbol <- Symbol?

MetaName <- '!' Symbol

Block <- EndOfLine? _ (ScriptBlock / NormalBlock)
NormalBlock <- '{' BlockProgram '}'
BlockProgram <- Program
ScriptBlock <- '{:' ScriptContents2 ':}'
ScriptContents2 <- (!':}' .)* 

AssignLine <- _ ('*' / Assignee) _ '=' _ Expression
Assignee <- AsmSymbol _

Lambda <- '[' FnArgs '->' EndOfLine? DelayedExpression ']'


Script <- '%{' ScriptContents '}%'
ScriptContents <- (!'}%' .)* 

Instruction <- Opcode Arg?
MacroCall <- Call

FnDef <- FnName '(' FnArgs ')'
FnName <- Symbol
FnArgs <- (FnArg (',' FnArg)*)?
FnArg <- _? Symbol _?

String <- _ ["] StringContents ["] _
StringContents <- (!["] .)*

Label <- (_ AsmSymbol ':') / AsmSymbol

Arg <- _ (ZRel / Acc / Imm / IndY / IndX / Ind / AbsX / AbsY / Abs)

IndX <- '(' Expression (TailX0 / TailX1)
TailX0 <- ')' _? ',' _? 'X'i
TailX1 <- ',' _? 'X'i _? ')'

IndY <- '(' Expression (TailY0 / TailY1)
TailY0 <- ')' _? ',' _? 'Y'i
TailY1 <- ',' _? 'Y'i _? ')'

Ind <- '(' Expression ')' &(!Operator)
Acc <- 'a'i &(![a-zA-Z0-9])
Abs <- LabelRef / Expression
AbsX <- Expression ',' _? 'X'i
AbsY <- Expression ',' _? 'Y'i

ZRel <- Expression ':' _? Expression ',' _? (LabelRef / Expression)

Imm <- '#' Expression

LabelRef <- '-'+ / '+'+

Opcode <- Symbol ('.' Suffix)?

Suffix <- [a-zA-Z0-9]+

HexNum <- ('$' / '0x') [0-9a-fA-F]+
Octal <- '0o' [0-7]+
Binary <- ('0b' / '%')  [01]+
Char <- '\'' . '\''
Decimal <- ([0-9]+ '.')? [0-9]+
Multi <- '0m' [0-3]+
Number <-  HexNum / Binary / Octal / Multi / Decimal / Char

~_ <- Space*
~WS <- Space+
~Space <- [ \u200b\t]

Symbol <- [_A-Za-z] [_A-Za-z0-9]*
DotSymbol <- ([._A-Za-z] [_A-Za-z0-9]*)
AsmSymbol <- LabelChar / ( DotSymbol ('[' Expression ']')? )

LabelChar <- '-' / '+' / '$'

~EndOfLine <- _ Comment? EOL
Comment <- ';' (!EOL .)*
EOL <- '\r\n' / '\r' / '\n'
EOT <- !.

Expression <- Expression2 Tern?

Expression2  <- Atom (Operator Atom)* {
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

Tern <- '?' DelayedExpression ':' DelayedExpression

Atom <- _? (Star / Unary / Unary2 / Number / String /
        Index / Lambda / ArrayLiteral / FnCall / Variable / '(' Expression ')') _?

Star <- '*'

ArrayLiteral <- '[' Expression (',' Expression)* ']'

Index <- Indexable '[' Expression? (IndexSep Expression?)? ']'

Indexable <- FnCall / Variable / Lambda

IndexSep <- ':'

Unary <- UnOp Atom
Unary2 <- UnOp2 Expression
UnOp <- ('!' / '~' / '-')
UnOp2 <- ('<' / '>')
FnCall <- Call
Call <- CallName '(' CallArgs ')'
CallName <- Symbol
CallArgs <- (_ CallArg (',' _ CallArg)*)?
CallArg <- (Symbol _? '=' !'=')? Expression
Operator <-  
        '&&' / '||' / '<<' / '>>' / '==' / '!=' /
        '>=' / '<=' /
        '+' / '-' / '*' / '/' / '%' / '\\' /
        '|' / '^' / '&' / '<' / '>'

Variable <- '.'? (Dollar / Symbol) ('.' Symbol)*
Dollar <- '$'
)";
