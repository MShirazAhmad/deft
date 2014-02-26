#!/usr/bin/python
from __future__ import division
# We need the following two lines in order for matplotlib to work
# without access to an X server.
import matplotlib, sys, os.path, sympy, numpy
if len(sys.argv) < 2 or sys.argv[1] != "show":
  matplotlib.use('Agg')
from scipy.optimize import leastsq
from sympy import pi, exp
import pylab, string

sigma = 2

# create variables to store latex / C++ code
latex_code = r"""% Code generated by plot-ghs.py
\documentclass{article}
\usepackage{breqn}
\begin{document}
"""
c_code = r"""// Code generated by plot-ghs.py
#include <math.h>
"""

# define variables / constants
variables = ['r', 'h_sigma', 'g_sigma']
positive_variables = ['kappa_0', 'kappa_1', 'kappa_2']
# expressions is a list of tuples, where each tuple is the name of a variable followed by the
# expression it is equal two, in terms of lambda expressions of the dict v.
expressions = [
  ('g_HS', lambda: 1 + v['h_sigma']*exp(-v['kappa_0']*v['zeta']) + v['h_sigma']*(v['kappa_0'] - 2*v['g_sigma'])*v['zeta']*exp(-v['kappa_1']*v['zeta']) + v['B']*v['zeta']**2*exp(-v['kappa_2']*v['zeta'])),
  ('B', lambda: 4*(v['kappa_2']**5/32/((v['kappa_2']**2/4 + 3*v['kappa_2']/2 + 3)))*((-1 + v['chi'])/(24*v['eta']) -
                                        v['h_sigma']*(2 + 2*v['kappa_0'] + v['kappa_0']**2)/v['kappa_0']**3 -
                                        v['h_sigma']*(v['kappa_0'] - 2*v['g_sigma'])*(v['kappa_1']**2 + 4*v['kappa_1'] + 2*3)/v['kappa_1']**4    )),
  ('chi', lambda: (1-v['eta'])**4/(1 + 4*v['eta'] + 4*v['eta']**2 - 4*v['eta']**3 + v['eta']**4)),
  ('eta', lambda: eta_expr),
  ('g_sigma', lambda: v['h_sigma'] + 1),
  ('zeta', lambda: (v['r'] - v['sigma'])/v['sigma']),
  ('R', lambda: v['sigma']/2),
  ('sigma', lambda: sympy.S(sigma))
]
l = []
expr = []
for x in expressions:
  l.append(x[0])
  expr.append(x[1])

v1 = dict((elem, sympy.symbols(elem)) for elem in l+variables)
v2 = dict((elem, sympy.symbols(elem, positive=True)) for elem in positive_variables)
v = dict(v1, **v2)

k0 = v['kappa_0']
k1 = v['kappa_1']
k2 = v['kappa_2']

h_sigma_expr = (1 - v['eta']/2)/(1 - v['eta'])**3 - 1
h_sigma_equation = sympy.Eq(v['h_sigma'], h_sigma_expr)

# this will return 3 expressions, 2 of which are complex
# eta_expressions = sympy.solve(h_sigma_equation, v['eta'], minimal=True)

# # get the real eta:
# eta_expr = None
# for i in xrange(len(eta_expressions)):
#   if sympy.ask(sympy.Q.real(eta_expressions[i].subs('h_sigma', 1))): # the 1 is arbitrary
#     eta_expr = eta_expressions[i]
#     break

# if eta_expr == None:
#   print 'Error: no real solutions for eta(h_sigma) found.'
#   exit(1)

k = (6*(9*v['g_sigma']**2 - sympy.sqrt(3*(27*v['g_sigma']**4 - 2*v['g_sigma']**3))))**(sympy.S(1)/3)
eta_expr = 1 - 1/k - k/(6*v['g_sigma'])

##################3



f = open('figs/ghs-analytics.tex', 'w')
f.write(latex_code)
f.close()


for i in xrange(len(expr)):
  latex_code += '\\begin{dmath}\n' + sympy.latex(sympy.Eq(v[l[i]], expr[i]())) + '\n\\end{dmath}\n'

# the expression for h_sigma is not included in the above list,
# so let's manually throw it into the latex
latex_code += '\\begin{dmath}\n' + sympy.latex(sympy.Eq(v['h_sigma'], h_sigma_expr)) + '\n\\end{dmath}\n'

# this loop unwraps the onion, getting ghs in terms of only the 3 kappas, h_sigma, and r
# we will wait on substituting h_sigma as a function of eta, though
for i in reversed(xrange(len(expr))):
  if l[i] == 'eta':
    eta_i = i
  else:
    temp = v[l[i]]
    v[l[i]] = expr[i]()
    # v[l[i]] = sympy.simplify(expr[i]() this makes it take way too long
    latex_code += '\\begin{dmath}\n' + sympy.latex(sympy.Eq(temp, v[l[i]])) + '\n\\end{dmath}\n'

ghs_s = expr[0]()

# Let us now verify that our contraints are met:
print "checking conditions on g(r):"

# --- g(sigma) == g_sigma?
gsigma = ghs_s.subs('r', sigma)
cor = 'correct' if gsigma == v['h_sigma'] + 1 else 'INCORRECT'
print '\tValue at contact is ' + cor + ': g(sigma) =', gsigma
latex_code += '\\begin{dmath}\n' + 'g(\sigma) = ' + sympy.latex(gsigma) + '\n\\end{dmath}\n'

# --- g'(sigma) == -h_sigma*g_sigma?
gprimesigma = ghs_s.diff('r').subs(v['r'], sigma).simplify()
cor = 'correct' if gprimesigma == -v['h_sigma']*(v['h_sigma'] + 1) else 'INCORRECT'
print '\tSlope at contact is ' + cor + ': g\'(sigma) =', gprimesigma
latex_code += '\\begin{dmath}\n' + 'g\'(\sigma) = ' + sympy.latex(gprimesigma) + '\n\\end{dmath}\n'

# --- 1 + n*int h(r)d^3r = nkT\chi_T

###################333333333333333

# now let's substitute in eta:
v['eta'] = eta_expr
ghs_s = ghs_s.subs('eta', v['eta']).subs('g_sigma', v['g_sigma'])
print '**********************\nghs_s is', ghs_s

#################################################################################################


# now that we have ghs defined, we want to do a best fit to find kappa_0, kappa_1, and kappa_2
lam = sympy.utilities.lambdify(('kappa_0', 'kappa_1', 'kappa_2', 'eta', 'r'),
                               ghs_s.subs('h_sigma', h_sigma_expr), 'numpy')

def evalg(x, eta, r):
  return lam(x[0], x[1], x[2], eta, r)

def read_ghs(base, ff):
  global able_to_read_file
  mcdatafilename = "%s-%4.2f.dat" % (base, ff)
  if (os.path.isfile(mcdatafilename) == False):
    print "File does not exist: ", mcdatafilename
    able_to_read_file = False
    return 0, 0

  mcdata = numpy.loadtxt(mcdatafilename)
  print 'Using', mcdatafilename, 'for filling fraction', ff
  r_mc = mcdata[:,0]
  n_mc = mcdata[:,1]
  ghs = n_mc/ff
  return r_mc, ghs

colors = ['r', 'g', 'b', 'c', 'm', 'k', 'y']
ff = numpy.array([.4, .3, .2, .1])

x = numpy.array([3.68, 2.16, 2.79])


# read data
able_to_read_file = True

ghs = [0]*len(ff)
eta = [0]*len(ff)

pylab.figure(1, figsize=(5,4))
pylab.axvline(x=sigma, color='k', linestyle=':')
pylab.axhline(y=1, color='k', linestyle=':')

for i in range(len(ff)):
    r_mc, ghs[i] = read_ghs("figs/gr", ff[i])
    if able_to_read_file == False:
        break
    pylab.figure(1)
    pylab.plot(r_mc, ghs[i], colors[i]+"-",label='$\eta = %.1f$'%ff[i])
    eta[i] = ff[i]
    r = r_mc

if able_to_read_file == False:
  pylab.plot(arange(0,10,1), [0]*10, 'k')
  suptitle('!!!!WARNING!!!!! There is data missing from this plot!', fontsize=25)
  pylab.savefig("figs/ghs-g2.pdf")
  pylab.savefig("figs/ghs-g-ghs2.pdf")
  exit(0)

# now do the least squares fit
def dist(x):
  # function with x[i] as constants to be determined
  R, ETA = pylab.meshgrid(r, eta)
  g = pylab.zeros_like(ETA)
  g = evalg(x, ETA, R)
  return pylab.reshape(g, len(eta)*len(r))

def dist2(x):
  return dist(x) - pylab.reshape(ghs, len(eta)*len(r))

ghsconcatenated = ghs[0]
for i in range(1,len(ff)):
  ghsconcatenated = pylab.concatenate((ghsconcatenated, ghs[i]))

etaconcatenated = [0]*len(r)*len(eta)
j = 0
while (j < len(eta)):
  i = 0
  while (i < len(r)):
    etaconcatenated[i + j*len(r)] = eta[j]
    i += 1
  j += 1

rconcatenated = [0]*len(r)*len(eta)
j = 0
while (j < len(eta)):
  i = 0
  while (i < len(r)):
    rconcatenated[i + j*len(r)] = r[i]
    i += 1
  j += 1

vals = pylab.zeros_like(x)

chi2 = sum(dist2(x)**2)
print "beginning least squares fit, chi^2 initial: %g" %chi2
vals, mesg = leastsq(dist2, x)
# round fitted numbers
digits = 2
vals = pylab.around(vals, digits)
chi2 = sum(dist2(vals)**2)
print "original fit complete, chi^2: %g" % chi2

toprint = True
for i in range(len(x)):
  print "vals[%i]: %.*f\t x[%i]: %g" %(i, digits, vals[i], i, x[i])

g = dist(vals)
gdifference = dist2(vals)

chisq = (gdifference**2).sum()
maxerr = abs(gdifference).max()
etamaxerr = 0
rmaxerr = 0
for i in xrange(len(gdifference)):
  if abs(gdifference[i]) == maxerr:
    etamaxerr = etaconcatenated[i]
    rmaxerr = rconcatenated[i]
K0 = vals[0]
K1 = vals[1]
K2 = vals[2]

def next_comma(ccode):
  """ returns next comma not counting commas within parentheses """
  deepness = 0
  for i in xrange(len(ccode)):
    if ccode[i] == ')':
      if deepness == 0:
        return -1
      else:
        deepness -= 1
    elif ccode[i] == '(':
      deepness += 1
    elif ccode[i] == ',' and deepness == 0:
      return i

def next_right_paren(ccode):
  """ returns next ")" not counting matching parentheses """
  deepness = 0
  for i in xrange(len(ccode)):
    if ccode[i] == ')':
      if deepness == 0:
        return i
      else:
        deepness -= 1
    elif ccode[i] == '(':
      deepness += 1

def fix_pows(ccode):
  """ A pointless optimization to remove unneeded calls to "pow()".
  It turns out not to make a difference in the speed of walls.mkdat,
  but I'm leaving it in, becuase this way we can easily check if this
  makes a difference (since I thought that it might). """
  n = string.find(ccode, 'pow(')
  if n > 0:
    return ccode[:n] + fix_pows(ccode[n:])
  if n == -1:
    return ccode
  ccode = ccode[4:] # skip 'pow('
  icomma = next_comma(ccode)
  arg1 = fix_pows(ccode[:icomma])
  ccode = ccode[icomma+1:]
  iparen = next_right_paren(ccode)
  arg2 = fix_pows(ccode[:iparen])
  ccode = fix_pows(ccode[iparen+1:])
  if arg2 == ' 2':
    return '((%s)*(%s))%s' % (arg1, arg1, ccode)
  if arg2 == ' 3':
    return '((%s)*(%s)*(%s))%s' % (arg1, arg1, arg1, ccode)
  if arg2 == ' 4':
    return '((%s)*(%s)*(%s)*(%s))%s' % (arg1, arg1, arg1, arg1, ccode)
  if arg2 == ' 5':
    return '((%s)*(%s)*(%s)*(%s)*(%s))%s' % (arg1, arg1, arg1, arg1, arg1, ccode)
  return 'pow(%s, %s)%s' % (arg1, arg2, ccode)

# finish printing to latex and c++ with the constants
c_code += r"""
const double kappa_0 = %.*f;
const double kappa_1 = %.*f;
const double kappa_2 = %.*f;

inline double gsigma_to_eta(const double g_sigma) {
  if (g_sigma <= 1) return 0;
  const double h_sigma = g_sigma - 1;
  return %s;
}


inline double radial_distribution(double g_sigma, double r) {
  if (g_sigma <= 1) return 1; // handle roundoff error okay
  if (r < %i) return 0;
  const double h_sigma = g_sigma - 1;
  return %s;
}
""" %(digits, K0, digits, K1, digits, K2, fix_pows(sympy.ccode(v['eta'])), v['sigma'], fix_pows(sympy.ccode(ghs_s)))

latex_code += r"""
\begin{dmath}
kappa_0 = %.*f
\end{dmath}
\begin{dmath}
kappa_1 = %.*f
\end{dmath}
\begin{dmath}
kappa_2 = %.*f
\end{dmath}
\end{document}
""" %(digits, K0, digits, K1, digits, K2)

f = open('figs/ghs-analytics.tex', 'w')
f.write(latex_code)
f.close()

f = open('figs/ghs-analytics.h', 'w')
f.write(c_code)
f.close()

# save fit parameters
outfile = open('figs/fit-parameters.tex', 'w')
outfile.write(r"""
\newcommand\maxerr{%(maxerr).2g}
\newcommand\etamaxerr{%(etamaxerr)g}
\newcommand\rmaxerr{%(rmaxerr).2g}
\newcommand\chisq{%(chisq).2g}
\newcommand\kappazero{%(K0)g}
\newcommand\kappaone{%(K1)g}
\newcommand\kappatwo{%(K2)g}
""" % locals())
outfile.close()

# now let's plot the fit
for i in range(len(ff)):
  pylab.figure(1)
  pylab.plot(r_mc, g[i*len(r):(i+1)*len(r)], colors[i]+'--')
  hsigma = (1 - 0.5*ff[i])/(1-ff[i])**3 - 1
  density = 4/3*pi*ff[i]
  rhs = (1-ff[i])**4/(1+4*ff[i]+4*ff[i]**2-4*ff[i]**3+ff[i]**4)/3
  #integral = hsigma*(1/a + x[0]*x[1]/())



  #print density, integral, rhs
  #print "ff: %.2f\t thing: %g" %(ff[i], 1 - rho*integral - rhs)
  pylab.figure(2)
  #plot(r_mc, gdifference[i*len(r):(i+1)*len(r)], colors[i]+'--')
  pylab.plot(r_mc, g[i*len(r):(i+1)*len(r)] - ghs[i], colors[i]+'-')
  # calculating integral:
  #mc:
  r_mc, ghs[i]
  integrand_mc = 4*pi*r_mc*r_mc*ghs[i]
  integrand_ours = 4*pi*r_mc*r_mc*g[i*len(r):(i+1)*len(r)]
  integral_mc = sum(integrand_mc)/len(integrand_mc)*(r_mc[2]-r_mc[1]) - 4/3*pi*sigma**3
  integral_ours = sum(integrand_ours)/len(integrand_ours)*(r_mc[2]-r_mc[1]) - 4/3*pi*sigma**3
  print("Int_mc: %6.3f, Int_ours: %6.3f, Diff: %6.3f" %(integral_mc, integral_ours, integral_ours-integral_mc))



  #plot(r_mc, numpy.abs(numpy.asarray(ghsconcatenated[i*len(r):(i+1)*len(r)]) - ghs[i]), color+'-')



pylab.figure(1)
pylab.xlim(0,6.5)
pylab.ylim(0., 3.5)
pylab.xlabel(r"$r/R$")
pylab.ylabel("$g(r)$")
pylab.legend(loc='best').get_frame().set_alpha(0.5)

pylab.tight_layout()
pylab.savefig("figs/ghs-g2.pdf")


pylab.figure(2)
pylab.xlim(2,6.5)
pylab.ylim(-.25, .25)
pylab.xlabel(r"$r/R$")
pylab.ylabel("|ghs - g|")
pylab.savefig("figs/ghs-g-ghs2.pdf")

pylab.axhline(y=0)
pylab.xlim(2,6.5)
pylab.legend(loc='best')
pylab.show()
