! Syntactically valid legacy Fortran for compiler parse tree testing
      program legacy_patterns
      use undefined_mod
      implicit none
      integer i
      integer j
      integer k, l
      real x
      integer mylabel
      integer idx
      character*5 name
      
      ! EQUIVALENCE with spacing (declaration)
      equivalence   (  i  ,   j  )

      ! COMMON blocks (declaration)
      common /block/ k, l

      ! Hollerith constant (declaration)
      data name / 5Hhello /

      ! Obsolete Statement function definition (before executable statements)
      f(y) = y * y + 1.0
      
      ! Executable statements begin here
      ! Arithmetic IF
      if (i) 10, 20, 30
 10   i = -1
 20   i = 0
 30   i = 1

      ! Computed GOTO without comma
      goto (100, 200, 300) i
 100  write(*,*) 'computed goto 100'
 200  write(*,*) 'computed goto 200'
 300  write(*,*) 'computed goto 300'

      ! ASSIGN & Assigned GOTO
      assign 200 to mylabel
      goto mylabel

      ! PAUSE statement
      pause "paused execution"

      ! Label DO loop
      do 50 idx = 1, 5
        write(*,*) idx
 50   continue

      ! Call to an undefined subroutine
      call undefined_sub(i)

      ! Multi-line continuation check
      write(*,*) &
        'testing statement continuation' &
        , i

      ! Explicit type assignment
      x = f(2.0)
      write(*,*) x

      end

      subroutine legacy_sub(arr)
      ! Assumed-size array
      real arr(*)
      
      write(*,*) arr(1)
      
      ! ENTRY statement
      entry alt_sub(arr)
      write(*,*) 'alt'
      
      end
