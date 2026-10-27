/* Classic Rexx source for the optional DSLSH highlighter; it is never run. */
options levelc
greeting = 'Hello, λ'
total = 0
do index = 1 to 3
  total = total + index
end index
if total = 6 then say greeting
else say 'Unexpected total:' total
/* Authored continuation punctuation and nested /* comment */ coverage. */
say 'Total:',
    total
call showtotal total
exit 0

showtotal: procedure
  parse arg amount
  say 'Routine received:' amount
  return
