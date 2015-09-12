" Vim syntax file
" Language:	Mu
" Maintainer: Christopher Haster <chaster@utexas.edu>
" URL: http://github.com/geky/mu

if exists("b:current_syntax")
  finish
endif


" Keywords in Mu
syn keyword muControl   if else while for and or
syn keyword muControl   break continue return
syn keyword muDefine    let fn
syn keyword muType      num str tbl fn_
syn keyword muIdent     nil _ inf e pi true false

" Comments
syn match muComment "#.*$"

" Numbers
syn match muNum "\<\d\+\(\.\d\+\)\?\([eE][+-]\?\d\+\)\?"
syn match muNum "\<0[xX]\x\+\(\.\x\+\)\?\([pP][+-]\?\x\+\)\?"
syn match muNum "\<0[oO]\o\+\(\.\o\+\)\?\([pP][+-]\?\o\+\)\?"
syn match muNum "\<0[bB][01]\+\(\.[01]\+\)\?\([pP][+-]\?[01]\+\)\?"

" Strings
syn region muStr start=+'+ end=+'+
syn region muStr start=+"+ end=+"+ contains=muEscape
syn match muEscape contained "\\[\\abfnrtv"]"
syn match muEscape contained "\\d\d\{3}"
syn match muEscape contained "\\o\o\{3}"
syn match muEscape contained "\\x\x\{2}"

" Various Regions
syn region muBlock start=+{+  end=+}+  transparent fold
syn region muTable start=+\[+ end=+\]+ transparent
syn region muParen start=+(+  end=+)+  transparent
syn match muEscape "\\$"


" Highlighting
hi def link muComment   Comment
hi def link muNum       Number
hi def link muStr       String
hi def link muEscape    Special
hi def link muControl   Statement
hi def link muDefine    Structure
hi def link muType      Function
hi def link muIdent     Function
hi def link muOp        Operator

let b:current_syntax = "mu"

