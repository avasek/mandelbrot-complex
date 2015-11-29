# mandelbrot-complex
Mandelbrot set generator which can use fractional and complex exponents in the recursive function

To use, download the files and run make

1. cd ~\Documents
2. git clone git://github.com/avasek/mandelbrot-complex.git Mandelbrot
4. cd Mandelbrot
5. make 

# Background
For this, the following basic recursive definition of the fractal is used, assuming the complex value C is the point being tested. The Mandelbrot set is traditionally defined for (a+bi) = 2.

```sh
  Base Case:
  Z(0) = C
  Recursive Case:
  Z(N+1) = Z(N)^(a+bi)+C
```

  The fractal is defined to be the set of points C for which the following limit is held.

```
lim N->Inf |Z(N)| < bound, bound > 0, bound element of the Reals.
```

  For the Mandelbrot set [(a+bi) = 2] is independant of the value of the bound, given that b >= 2.

  Traditionally, the Mandelbrot fractal is displayed as a colorful image. This is obtained by noting the value of N and |Z(N)| for the first value that is larger than the bound. The following formula is used to generate a value in the range (0,1), based on the Taylor expansion shown at
  http://linas.org/art-gallery/escape/escape.html.
```
  modN = N + 1 - log(log(|Z(N)|)) / log(r^a * e^(-b * theta))
```
  
  The returned value is modN divided by the maximum number of iterations allowed by the program.

# Examples
```
  Recursive Case:
  Z(N+1) = Z(N)^2+C
```
![Standard Mandelbrot Image](https://github.com/avasek/mandelbrot-complex/blob/master/Examples/Dimension:%201920x1080%2C%20Center:%20-0.5000%2B0.0000i%2C%20Scale:%202.00e-03%2C%20Exp:%202.00e%2B00%2B0.00e%2B00i%2C%20Branch%20set.png)

```
  Recursive Case:
  Z(N+1) = Z(N)^2.2+C
```
![Modify the Real Exponent](https://github.com/avasek/mandelbrot-complex/blob/master/Examples/Dimension:%201920x1080%2C%20Center:%20-0.5000%2B0.0000i%2C%20Scale:%202.00e-03%2C%20Exp:%202.20e%2B00%2B0.00e%2B00i%2C%20Branch%20set.png)

```
  Recursive Case:
  Z(N+1) = Z(N)^(2+0.01i)+C
```
![Modify the Imaginary Exponent](https://github.com/avasek/mandelbrot-complex/blob/master/Examples/Dimension:%201920x1080%2C%20Center:%20-0.5000%2B0.0000i%2C%20Scale:%202.00e-03%2C%20Exp:%202.00e%2B00%2B1.00e-02i%2C%20Branch%20not%20set.png)


# Using the Mandelbrot Set Generator

After calling make, the following flags have been created to determine the action of the generating code:

| Flag | Default | Description |
|------|---------|-------------|
| -w   | 1920 | Width of the fractal in pixels |
| -h   | 1080 | Height of the fractal in pixels |
| -s   | 0.002 | Size of one pixel in the complex plane |
| -r   | -0.5 | Center of the image, Real axis |
| -i   | 0.0 | Center of the image, Imaginary axis |
| -a   | 2.0 | Real component of the exponent |
| -b   | 0.0 | Imaginary component of the exponent |
| -t   | 4 | Number of threads |

### Fine Details - Branch Cuts

  One more discussion must be had before generating using these formulas, and that involves branch cuts.
  Branch cuts are important when discussing the value returned by the arctangent function. The function is
  defined such that:
```
       tan(x) = y <==> arctan(y) = x
```
  However, the following is true of the tangent function:
  ```
  tan(x) = tan(x + 2*pi) = tan(x + 4*pi) = tan(x - 2*pi) = .... = tan(x+2*pi*i), i-> integer
  ```

  The arctangent function traditionally returns a value in the range (-pi, pi]. However, due to the property of the tangent function, the value returned by the arctangent function can be in any range (x-pi, x+pi] for any value of x. The range returned is known as the branch, and the extreme values of the range are known as the branch cut (Note that in polar form, the angles x+pi and x-pi are coincident in the polar plane).

  Unfortunately for complex exponentiation, the choice of which value of x to use to center the range will affect the result that is obtained in a very meaningful way

  For the generation of fractals, there are many ways to pick a meaningful branch. Below are two methods
```
  1. x = -b
           Use the imaginary component of the exponent to set the branch 

  2. x = Arg(c)
           Use the argument of the original point to determine the branch
```

  General Statements on branch cut:
  Generally I have used this modified exponent Mandelbrot set generator for three general cases
  and have the following generalizations about the "best" branch cut mode
```
  1. Integer values of a and b=0 (ie. Z(N+1) = Z(N)^4 + C)
      Here the branch cut method makes little to no difference

  2. Small values for b and a=2 ((ie. Z(N+1) = Z(N)^(2+0.01i) + C))
      I find the images look best with the first method of branch cut (flag not set)

  3. Non-integer values of a and b=0
     I have liked the images with the second method of the branch cut (flag is set)
```
