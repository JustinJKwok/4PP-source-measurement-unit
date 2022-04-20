import numpy as np
from pprint import pprint


def main():
    R_c = 50000.0 # Contact resistance 10k+
    R_sam_min = 1.0 # Sample resistance between 2 probes
    R_sam_max = 30.0
    gain = 400.0 # Gain of measured voltage across inner probes. 100, 200, 400, depending on acceptable Vsam, and Rc/Rsam, and Voutmax
    V_out_max = 36.0 # Maximum voltage op amp can output
    V_measure = 0.2 # The minimum voltage that is acceptable for measurements
    
    R_min = 2*R_c+3.0*R_sam_min
    R_max = 2*R_c+3.0*R_sam_max    
    R_sams_outer = np.logspace(np.log10(R_min),np.log10(R_max),num=50)
    
    res = []
    for R_sam in R_sams_outer:
        res.append((R_sam, measure(R_sam, R_c, gain, V_out_max, V_measure)))
        
    print("")
    print("Results")
    pprint(res)

def measure(R_sam_outer, R_c, gain, V_out_max, V_measure):
    print('======================================================')
    print('Solving for R_sam_outer = ' + str(R_sam_outer))
    print("R_c contact = " + str(R_c))
    print("Gain for V_sam_inner = " + str(gain))
    
    factor = 2.0
    
    current = 10e-9 / factor # Start with lowest current 
    
    V_sam_inner = 0
    while V_sam_inner < V_measure: #acceptable limit to stop searching
        print('')
        print('******Iterating******')
        current = current * factor
        print("Current = " + str(current))
        
        if current < 1e-7:
            R_series = 1e7
        elif current < 1e-6:
            R_series = 1e6
        elif current < 1e-5:
            R_series = 1e5
        elif current < 1e-4:
            R_series = 1e4
        elif current < 1e-3:
            R_series = 1e3
        else:
            print("reached smallest R_series")
            return (False, "reached smallest R_series")
            

        print("R_series = " + str(R_series))
        
        # Set input voltage to achieve desired current
        V_dac = current * R_series
        dac_out = V_dac / 5.0 * 4095

        print("V_dac = " + str(V_dac))
        print("dac_out = " + str(dac_out))
        
        # Correct choice of R_series per current range should avoid this
        if V_dac > 5.0:
            print("Reached V_dac limit of 5.0")
            return (False, "Reached V_dac limit of 5.0")
    
        # Calc current, Optional measurement in real life
        V_adc = V_dac
        I_sam = V_adc / R_series
        
        # measure inner voltage (done backwards in real)
        V_sam_outer = I_sam * R_sam_outer
        R_sam_inner = (R_sam_outer - 2.0 * R_c) / 3
        V_sam_inner = R_sam_inner * I_sam * gain

        print("R_sam_inner = " + str(R_sam_inner))
        print("V_sam_inner = " + str(V_sam_inner))
        
        # check V out of op amp
        V_out = V_sam_outer + V_adc
        
        print("V_out = " + str(V_out))
        
        if V_out > V_out_max:
            if current == 10e-9:
                print("V_out exceeded, overall R too high")
                return (False, "V_out exceeded, overall R too high")
            else:
                print("V_out exceeded, Rc >> Rfilm")
                return (False, "V_out exceeded, Rc >> Rfilm")
        
    print("Search ended successfully")
    return (True, "Search ended successfully")
    
if __name__ == "__main__":
    main()
        
    
    
    