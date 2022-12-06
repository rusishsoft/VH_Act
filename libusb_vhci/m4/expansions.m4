AC_DEFUN([MY_CHECK_WS],
[dnl
if expr index "x$PWD" ' ' >/dev/null 2>&1; then
	AC_MSG_ERROR([whitespaces in builddir are not supported])
fi
if expr index "x$[]0" ' ' >/dev/null 2>&1; then
	AC_MSG_ERROR([whitespaces in srcdir are not supported])
fi
])dnl
dnl
AC_DEFUN([MY_ARG_ENABLE],
[dnl
m4_do(
	[AC_ARG_ENABLE([$1],
		[AS_HELP_STRING(
			[--m4_if([$2<m4_unquote(m4_normalize([$4]))>], yes<>, dis, en)able-$1[]m4_case(
				m4_join(|, m4_unquote(m4_normalize([$4]))),
				yes|no, [],
				no|yes, [],
				yes,    [],
				no,     [],
				[],     [],
				        [@<:@=ARG@:>@]
			)],
			[$3 ]m4_case(
				m4_join(|, m4_unquote(m4_normalize([$4]))),
				yes|no, [[ ]],
				no|yes, [[ ]],
				yes,    [[ ]],
				no,     [[ ]],
				[],     [[ ]],
				        [[ARG is one of: ]m4_normalize([$4])[ ]]
			)m4_if(
				[$2<m4_unquote(m4_normalize([$4]))>],
				yes<>,
				[(enabled by default)],
				[(default is $2)]
			))],
		[enable_[]m4_translit([$1], -, _)_expl=yes],
		[enable_[]m4_translit([$1], -, _)=$2 enable_[]m4_translit([$1], -, _)_expl=""])],
	[AS_CASE(
		["$enable_[]m4_translit([$1], -, _)"],
		[m4_join(|, m4_ifval(m4_normalize([$4]), m4_normalize([$4]), [yes, no]))],
		[# ok]m4_newline,
		[AC_MSG_ERROR([the value '$enable_[]m4_translit([$1], -, _)' is invalid for --enable-$1])]
	)])]
)dnl
