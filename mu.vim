" Vim syntax file
" Language:	Mu
" Maintainer: Christopher Haster <chaster@utexas.edu>
" URL: http://github.com/geky/mu

if exists("b:current_syntax")
  finish
endif


" Keywords in Mu
syn keyword muControl if else while for
syn keyword muBranch break continue return
syn keyword muLet let
syn keyword muType num str tbl fn
syn keyword muReference args this global op
syn keyword muValue nil nan inf

" Comments
syn match muSingleComment "`[^`]*`\?"
syn region muMultiComment start=+``\z(`*\)+ end=+``\z1+

" Numbers
syn match muDecNum "\d\+\(\.\d\+\)\?\([eE][+-]\?\d\+\)\?"
syn match muHexNum "0[xX]\x\+\(\.\x\+\)\?\([pP][+-]\?\x\+\)\?"
syn match muOctNum "0[oO]\o\+\(\.\o\+\)\?\([pP][+-]\?\o\+\)\?"
syn match muBinNum "0[bB][01]\+\(\.[01]\+\)\?\([pP][+-]\?[01]\+\)\?"

" Strings
syn match muEscapes contained "\\\([\\abfnrtv]\|d\d\{3}\|o\o\{3}\|x\x\{2}\)"
syn region muSingleString start=+'+ end=+'+
syn region muDoubleString start=+"+ end=+"+ contains=muEscapes

" Various Regions
syn region muBlock start=+{+  end=+}+  transparent fold
syn region muTable start=+\[+ end=+\]+ transparent fold
syn region muParen start=+(+  end=+)+  transparent


" Highlighting
hi def link muSingleComment Comment
hi def link muMultiComment Comment
hi def link muDecNum Number
hi def link muHexNum Number
hi def link muOctNum Number
hi def link muBinNum Number
hi def link muSingleString String
hi def link muDoubleString String
hi def link muEscapes SpecialChar
hi def link muLet Type
hi def link muType Type
hi def link muReference Identifier
hi def link muValue Identifier
hi def link muControl Conditional
hi def link muBranch Statement

let b:current_syntax = "mu"

