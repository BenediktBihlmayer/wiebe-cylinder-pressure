[README.md](https://github.com/user-attachments/files/34945325/README.md)
# Engine Pressure and Crank Mechanism Force Model

A C++ program for evaluating a prescribed cylinder-pressure curve and
crank mechanism forces as functions of crank angle. The pressure model
combines a Gaussian base-pressure curve with a Wiebe function.

This is an educational engineering model reconstructed from a code listing.
It does not solve the thermodynamic energy balance of an engine cycle.

## Features

- Interactive parameter input, with default values
- Input expressions such as `pi/180`, `2*pi` and `10*pi/180`
- Wiebe-based burned-fraction calculation
- Prescribed cylinder-pressure calculation
- Gas forces and oscillating and rotating mass-force calculations
- Radial, tangential and Cartesian force components
- CSV output and an input-parameter record

## Requirements

A C++17-compatible compiler, such as GCC or Clang. No external libraries
are required.

## Build and Run

Save the source as `motor_kraftmodell.cpp` and run these commands
from the directory containing the file:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic motor_kraftmodell.cpp -o motor_kraftmodell
./motor_kraftmodell
```

These run instructions apply to Linux and macOS.

Press Enter at a prompt to accept its default value. Numerical input
accepts decimal points or decimal commas and supports addition,
subtraction, multiplication, division and the constant pi.
Parenthesized expressions are not supported.

CSV output uses a decimal point. The field separator can be a semicolon
or a comma.

## Coordinates and Units

The crank angle is measured in radians, with top dead centre at zero.
Combustion start and duration are entered in degrees and converted
internally to radians. Other inputs use SI units.

The default angle range is 0 to 2π. This is one crankshaft revolution,
not a complete four-stroke cycle. The pressure function is not periodic.

## Pressure Model

### Wiebe Function

Within the combustion interval:

```math
x_b(\varphi) = 1 - \exp\left[-a\left(\frac{\varphi-\varphi_0}{\Delta\varphi_c}\right)^{m+1}\right]
```

The code sets the burned fraction to zero before combustion starts
and to one after the combustion interval ends. At the end itself,
the expression gives 1 − exp(−a), creating a small jump immediately
after the endpoint.

### Prescribed Pressure Curve

```math
p_{\mathrm{base}}(\varphi) = p_{\mathrm{int}} + \left(p_{\mathrm{TDC,base}}-p_{\mathrm{int}}\right)\exp\left[-\left(\frac{\varphi}{\sigma}\right)^2\right]
```

```math
p_z(\varphi) = \max\left\{p_{\mathrm{int}},\ p_{\mathrm{base}}(\varphi)\left[1+\left(\frac{p_{\max}}{p_{\mathrm{TDC,base}}}-1\right)x_b(\varphi)\right]\right\}
```

Here, σ = 40π/180 rad. The pressure expression shown assumes a positive
base pressure at top dead centre. For a nonpositive value, the source
uses a pressure ratio of one.

The parameter p_max scales the pressure curve. It does not guarantee
that the calculated peak equals p_max.

## Force Calculation

The gas force uses the pressure difference across the piston:

```math
F_Z = (p_z-p_0)A_k
```

The reconstructed oscillating mass-force expression is:

```math
F_o = m_o r \omega^2\left(\cos\varphi+\lambda\cos^2\varphi\right)
```

This is the expression in the supplied listing. Its squared-cosine
term requires review before the model is used for quantitative work.

The connecting-rod angle and crank force components are calculated as:

```math
\beta=\arcsin(\lambda\sin\varphi),\qquad F_K=F_Z+F_o
```

```math
F_R=F_K\frac{\cos(\varphi+\beta)}{\cos\beta},\qquad F_T=F_K\frac{\sin(\varphi+\beta)}{\cos\beta}
```

The code resolves these forces into Cartesian components, adds its
rotating-mass contributions and exports the resulting vector magnitudes.
Force signs and reference frames must be checked against the intended
mechanical free-body diagram.

## Default Parameters

| Parameter | Value | Unit |
|---|---:|---|
| Start angle | 0.0 | rad |
| End angle | 2π | rad |
| Angular step | π/180 | rad |
| Intake pressure | 180000.0 | Pa |
| Exhaust pressure, currently unused in calculations | 120000.0 | Pa |
| Reference pressure | 100000.0 | Pa |
| Pressure scaling parameter p_max | 16000000.0 | Pa |
| Base pressure at top dead centre | 5000000.0 | Pa |
| Combustion start | -5.0 | deg |
| Combustion duration | 45.0 | deg |
| Wiebe parameter a | 6.9 | dimensionless |
| Wiebe exponent m | 2.0 | dimensionless |
| Piston area | 0.005153 | m² |
| Crank radius | 0.04775 | m |
| Connecting-rod length | 0.144 | m |
| Rod ratio λ | r/l | dimensionless |
| Angular velocity | 200.0 | rad/s |
| Connecting-rod mass | 0.6 | kg |
| Connecting-rod centre-of-mass parameter | 0.072 | m |
| Rotating connecting-rod mass | 0.3 | kg |
| Oscillating mass | 1.062 | kg |
| Crank-web mass | 1.6 | kg |
| Additional crankpin mass | 0.0 | kg |
| Crank-web radius | 0.04775 | m |

These are model inputs, not independently verified manufacturer data.
The angular velocity corresponds to approximately 1909.86 rpm.

The rod ratio and crank-web radius are initialized from the default
geometry. They remain separate inputs and are not automatically updated
when another geometric parameter is changed.

## Output Files

Files are written to the current working directory.

| File | Contents |
|---|---|
| `pz_fm_phi.csv` | Angle, pressure, burned fraction and force results |
| `input.dat` | Parameter record from the current run |

The CSV filename is configurable. Existing output files with the same
names are overwritten. The program writes input.dat but does not read
parameters from that file.

### CSV Columns

| Column | Meaning |
|---|---|
| phi_rad | Crank angle in radians |
| phi_deg | Crank angle in degrees, optional |
| p_Pa, p_bar | Cylinder pressure in Pa and bar |
| xb | Wiebe burned fraction, optional |
| beta_rad | Connecting-rod angle |
| Fo_N | Oscillating mass-force term |
| FZ_N | Gas force |
| FK_N | Combined piston-force term |
| FR_N, FT_N | Radial and tangential crank force components |
| FPR_N | Rotating connecting-rod mass-force term |
| FKWr_N | Additional rotating-mass force term |
| FKLx_N, FKLy_N | Calculated crankpin-force components |
| FMx_N, FMy_N | Calculated shaft-force components |
| FKL_N, FM_N | Resultant force magnitudes |

## Status and Limitations

Compilation and numerical verification of the reconstructed version
have not been confirmed. No validated reference results are included.

Known limitations of the supplied model:

- The pressure curve is prescribed. There is no energy balance,
  heat-transfer model, gas-exchange model or volume-based compression
  and expansion calculation.
- The exhaust-pressure input is stored but is not used by the pressure
  function.
- The oscillating mass-force expression contains cos²(φ).
  Its intended derivation needs verification.
- A rotating connecting-rod contribution enters through mpr and again
  through mp(l − rp)/l in the additional rotating-force term. This
  may double-count mass in the final shaft-force calculation.
- The force decomposition and sign conventions require a consistent
  mechanical derivation.
- The default interval does not include the negative combustion-start
  angle, so combustion is already underway at the first output point.
- The angle loop permits a final point up to half a step beyond the
  requested end angle when the interval is not divisible by the step.

The reconstruction repairs formatting and syntax, adds basic input
checks and uses decimal points for output. It preserves the supplied
physical formulas. The model should be reviewed and verified before
its output is used for engineering decisions.

## Author

Benedikt Bihlmayer  
Mechanical Engineering
