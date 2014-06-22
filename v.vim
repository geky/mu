" Vim syntax file
" Language:	V
" Maintainer: Christopher Haster <chaster@utexas.edu>
" URL: http://github.com/geky/v

if exists("b:current_syntax")
  finish
endif


" Keywords in V
syn keyword vControl if else while for
syn keyword vBranch break continue return
syn keyword vLet let
syn keyword vType num str tbl fn
syn keyword vReference args this global op
syn keyword vValue nil nan inf

" Comments
syn match vSingleComment "`[^`]*`\?"
syn region vMultiComment start=+``\z(`*\)+ end=+``\z1+

" Numbers
syn match vDecNum "\d\+\(\.\d\+\)\?\([eE][+-]\?\d\+\)\?"
syn match vHexNum "0[xX]\x\+\(\.\x\+\)\?\([pP][+-]\?\x\+\)\?"
syn match vOctNum "0[oO]\o\+\(\.\o\+\)\?\([pP][+-]\?\o\+\)\?"
syn match vBinNum "0[bB][01]\+\(\.[01]\+\)\?\([pP][+-]\?[01]\+\)\?"

" Strings
syn match vEscapes contained "\\\([\\abfnrtv]\|d\d\{3}\|o\o\{3}\|x\x\{2}\)"
syn region vSingleString start=+'+ end=+'+
syn region vDoubleString start=+"+ end=+"+ contains=vEscapes

" Various Regions
syn region vBlock start=+{+  end=+}+  transparent fold
syn region vTable start=+\[+ end=+\]+ transparent fold
syn region vParen start=+(+  end=+)+  transparent


" Highlighting
hi def link vSingleComment Comment
hi def link vMultiComment Comment
hi def link vDecNum Number
hi def link vHexNum Number
hi def link vOctNum Number
hi def link vBinNum Number
hi def link vSingleString String
hi def link vDoubleString String
hi def link vEscapes SpecialChar
hi def link vLet Type
hi def link vType Type
hi def link vReference Identifier
hi def link vValue Identifier
hi def link vControl Conditional
hi def link vBranch Statement

let b:current_syntax = "v"

