! Legacy Fortran test fixture for modernization advisor
      program legacy_patterns
      implicit none
      integer i

      if (i .lt. 0) 10, 20, 30
 10   i = -1
 20   i = 0
 30   i = 1

      goto (100, 200, 300), i
 100  write(*,*) 'computed goto 100'
 200  write(*,*) 'computed goto 200'
 300  write(*,*) 'computed goto 300'

      equivalence (i, j)
      integer j

      common /block/ k, l
      integer k, l

      x = 1.0
      write(*,*) x

      end
