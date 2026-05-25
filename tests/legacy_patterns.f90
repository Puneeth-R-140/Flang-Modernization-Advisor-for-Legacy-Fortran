! Legacy Fortran test fixture for modernization advisor
      program legacy_patterns
      implicit none
      integer i
      integer j
      integer k, l
      real x
      
      ! Obsolete Statement function definition (before executable statements)
      f(y) = y * y + 1.0
      
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
