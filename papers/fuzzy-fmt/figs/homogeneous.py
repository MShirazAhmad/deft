#!/usr/bin/python

from __future__ import division
# We need the following two lines in order for matplotlib to work
# without access to an X server.
import matplotlib
matplotlib.use('Agg')
from pylab import *
from scipy.special import erf

#Constants and variables
#k_b = 8.6173324*10**(-5) # in eV
dT = .001
#Temp_max = 600 #in Kelvin
#Temp = arange(.001, .1 + dT/2, dT)
V0 = 1
#betaV0 = V0/Temp
R = 1# in Angstroms
density = arange(0, .8 - .001/2, .001)/(4*pi/3)
#gamma = 2*((sqrt(pi*betaV0)+sqrt(pi*betaV0-16*sqrt(betaV0)))/8)**2
#sg = sqrt(gamma)

Temp = 0.00001
while Temp <= .011:
  betaV0 = V0/Temp
  gamma = 2*((sqrt(pi*betaV0)+sqrt(pi*betaV0-16*sqrt(betaV0)))/8)**2
  sg = sqrt(gamma)
  #Integrals for the different weighted densities
  W3 = (-pi*R/(3*gamma**(3/2)*(sqrt(pi*gamma) -1)))*(2*sg*(8*(1 + gamma) - exp(-gamma)*(2*gamma+5))-sqrt(pi)*(4*gamma**2+12*gamma+3)*erf(sg))

  W2 = (2*pi*R**2/(gamma*(sqrt(pi*gamma)-1)))*(sqrt(pi*gamma)*(2*gamma + 3)*erf(sg) - 6*gamma+2*exp(-gamma)*(gamma + 1) -  2)

  W1 = R/(2*sg*(sqrt(pi*gamma)-1))*(sqrt(pi)*(2*gamma + 1)*erf(sg) + sg*(4-6*exp(-gamma)))

  W0 = (sg/(sqrt(gamma*pi)-1))*(sqrt(pi)*erf(sg) + (exp(-gamma)-1)/sg)

  #Weighted densities
  n0 = density*W0
  n1 = density*W1
  n2 = density*W2
  n3 = density*W3

  Phi_1 = -n0*log(1-n3)
  Phi_2 = n1*n2/(1-n3)
  Phi_3 = n2**3/3/(8*pi*(1-n3)**2)

  Phi = Phi_1 + Phi_2 + Phi_3

  alpha = 1-n3
  #dPhi_dn = Phi_1/density + density*W2*(W3 + 2*W1)/alpha + density**2*W2*(3*W2**2 + W1*W3)/(alpha**2) + 2*(density*W2)**3*W3/(24*pi*alpha**3) 
  dPhi1_dn = Phi_1/density + density*W0*W3/alpha
  dPhi2_dn = (W1*n2+W2*n1)/alpha + n1*n2/alpha**2*W3
  dPhi3_dn = n2**2/(8*pi*alpha**2)*W2 + n2**3/12/pi/alpha**3*W3
  dPhi_dn = dPhi1_dn + dPhi2_dn + dPhi3_dn

  pressure = Temp*(density*dPhi_dn - Phi) + density*Temp 
  plot(density*(4*pi/3),pressure/Temp, label = 'T/V0=%.2e' %Temp)
  Temp = Temp*sqrt(10)

eta = density*4*pi/3
#P_cs = density*.001*(1+eta+eta**2-eta**3)/(1-eta)**3
P_cs = density*.001*(1+eta+eta**2)/(1-eta)**3
plot(eta,P_cs/.001, 'k',linewidth=2, label = 'Hard spheres')
#mcdata = loadtxt('figs/mc-soft-homogenous-20-382-1.00000.dat.prs')
#plot(mcdata[:,1],mcdata[:,0],'*')
xlabel('Packing fraction')
ylabel('Pressure/Temp')
legend(loc = 'best')
savefig('figs/p-vs-packing.pdf', bbox_inches=0)