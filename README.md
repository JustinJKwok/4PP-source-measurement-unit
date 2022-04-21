# 4PP-source-measurement-unit
An inexpensive current source and voltage measuring unit for 4 point probe measurements. Designed for linear 4 point probe, but works with any 4 point probe or 4-wire measurements.

Inspired by https://doi.org/10.3390/ma10020110 but uses different design and code.

## Details
This current source uses an enhanced Howland current source with buffered non-inverting feedback. Several series resistors are selectable using relays to expand the range of current from 10 nA to ~10 mA. Current is sourced through the outer 2 probes while voltage is measured across the inner 2 probes using an instrumentation amplifier. Relays are connected to the 2 outer probes to allow reversing of the current direction. DAC output feeds into the Howland current source and an ADC is used to measure the voltage from the in-amp. An optional 16-bit ADC is used although the Arduino internal ADC is fine.

## Usage
Arduino sketch coming soon

To ensure voltage compliance a python script is included to determine whether an acceptable voltage can be reached across the 2 inner probes. Two cases to watch out for:
- when the total resistance of the sample+contacts is so large that an acceptable voltage cannot be reached
- when the contact resistance >> the sample resistance.

To solve this:
- Increase op amp supply voltage to prevent railing
- Adjust series resistors to change accessible current range
- Adjust DAC ouput range to change accessible current range
- Lower the acceptable measured voltage
- Increase in-amp gain

## Scheme
<img src="https://github.com/JustinJKwok/4PP-source-measurement-unit/blob/master/smu_scheme.PNG">
