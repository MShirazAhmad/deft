#!/usr/bin/python

from __future__ import division
from math import pi       # REALLY don't need all of math
import os, numpy as np, sys

#os.system("fac ../../../free-energy-monte-carlo")

R=1

i= 0
# RG recursion level

ww=1.3
#arg ww = [1.3, 1.5, 2.0, 3.0]

L=5
#arg L = [5.0]

Ns = list(xrange(0,2))
#arg Ns = [range(2,10)]

Overwrite = False
if '-O' in sys.argv:  # Check for overwrite flag in arguments
    Overwrite = True

def free_energy_over_kT_to_ff(free_energy): # Find an ff that corresponds to given free energy
    # bisection
    n = 0
    low,high = 0,1
    resolution = 1E-8
    while n < 500: #better value?
        ff_guess = (low+high)/2
        free_energy_next =  (4*ff_guess - 3*ff_guess**2)/(1-ff_guess)**2
        if abs(free_energy_next - free_energy) < resolution:
            return ff_guess
        elif free_energy_next > free_energy:
            high = (high+low)/2
        else:
            low = (high +low)/2
        n+=1
    return ff

approximate_free_energies = np.arange(np.log(2), 20, np.log(2))
ffs = np.zeros_like(approximate_free_energies)
for k in xrange(len(ffs)):
    ffs[k] = free_energy_over_kT_to_ff(approximate_free_energies[k])
   
for N in Ns:
    dirname = 'scrunched-ww%4.2f-L%04.2f/i%01d/N%03d/absolute' % (ww,L,i,N)
    os.system('mkdir -p '+dirname)
    if Overwrite:
        os.system('rm -r '+dirname)

    success_ratios = []
    all_total_checks = []
    all_valid_checks = []
    steps = 20 # Need a better value for this
    step_size = 0.05 # This too
    steps = 20
    sim_iters = 1000000
    sc_period = int(max(10, 1*N*N/10))

    # ffs = np.zeros(steps+1)
    # ffs[0] = (4/3.0)*pi*R**3/L**3                # Deprecated; kept for posterity.
    # for j in xrange(steps):
    #     if j!= 0 and j % 4 == 0:
    #         step_size = step_size * 0.5
    #     ffs[j+1] = ffs[j] + step_size

   
    for j in xrange(len(ffs)-1):
        filename = '%05d' % (j)

        if not os.path.isfile(dirname+'/'+filename+'.dat'):
           # cmd = 'srun -J %s' % filename
            cmd = ' ../../../free-energy-monte-carlo'
            cmd += ' --ff %g' % ffs[j]
            cmd += ' --sc_period %d' % sc_period
            cmd += ' --iterations %d' % sim_iters
            cmd += ' --filename %s' % filename
            cmd += ' --data_dir %s' % dirname
            cmd += ' --ff_small %g' % ffs[j+1]
            cmd += ' --N %d' % N
            cmd += ' > %s/%s.out 2>&1 &' % (dirname, filename)
            print("Running with command: %s" % cmd)
            os.system(cmd)
        else:
            print("You're trying to overwrite files, use the flag -O in order to do so.")