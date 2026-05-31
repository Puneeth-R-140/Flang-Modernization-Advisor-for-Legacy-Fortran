! Legacy Fortran test fixture for modernization advisor
      program legacy_patterns
      implicit none
      use undefined_mod
      integer i
      integer j
      integer k, l
      real x
      integer mylabel
      integer idx
      character*5 name
      
      ! Obsolete Statement function definition (before executable statements)
      f(y) = y * y + 1.0
      
      ! Hollerith constant
      data name / 5Hhello /
      
      ! Arithmetic IF
      if (i) 10, 20, 30
 10   i = -1
 20   i = 0
 30   i = 1

      ! Computed GOTO without comma (optional in standard)
      goto (100, 200, 300) i
 100  write(*,*) 'computed goto 100'
 200  write(*,*) 'computed goto 200'
 300  write(*,*) 'computed goto 300'

      ! EQUIVALENCE with spacing
      equivalence   (  i  ,   j  )

      ! COMMON blocks
      common /block/ k, l

      ! ASSIGN & Assigned GOTO
      assign 200 to mylabel
      goto mylabel

      ! PAUSE statement
      pause "paused execution"

      ! Label DO loop
      do 50 idx = 1, 5
        write(*,*) idx
 50   continue

      ! Call to an undefined subroutine to trigger call graph validation
      call undefined_sub(i)

      ! Multi-line continuation check
      write(*,*) &
        'testing statement continuation' &
        , i

      ! Explicit type assignment
      x = f(2.0)
      write(*,*) x

      end

      ! A separate subroutine lacking IMPLICIT NONE to verify program unit implicit typing warning
      subroutine legacy_sub(arr)
      ! Assumed-size array
      real arr(*)
      
      write(*,*) arr(1)
      
      ! ENTRY statement
      entry alt_sub(arr)
      write(*,*) 'alt'
      
      end
