# OpenAVNS

## Purpose
To develop an open-source, open-architecture auricular vagus nerve stimulator under a distributed development model.
 
Auricular vagus nerve stimulation (aVNS) is a method of neuromodulation that uses electrical pulses applied to nerves near the ear to create a vagus nerve response. This response can lead to reduced sympathetic (fight or flight) and increased parasympathetic (relaxation / everything ok) tone. Adjusting vagal tone using aVNS has been explored by clinical researchers for a number of potential therapeutic applications, including for depression1, opioid withdraws2, chronic pain3, and gastrointestinal issues4. In addition, aVNS has been explored in wellness for applications such as stress reduction5. We believe an open-source, open-architecture aVNS system would be helpful for users, students, medical researchers, and community members around the world to explore aVNS and to learn more about developing neuromodulation devices under an open source model.
 
1.        Tan, Chaoren et. al. 2023. https://pubmed.ncbi.nlm.nih.gov/37230264/
2.        Tirado, Carlos F. et. al. 2022. https://pmc.ncbi.nlm.nih.gov/articles/PMC9385243/
3.        Duff, Irina T. et. al.  2024. https://pmc.ncbi.nlm.nih.gov/articles/PMC11543973/
4.        Veldman, Fleur et. al. 2025. https://pmc.ncbi.nlm.nih.gov/articles/PMC11769675/
5.        Sanches-Perez, Jesus et. al. 2023. https://pmc.ncbi.nlm.nih.gov/articles/PMC10512834/
 
## Principles of OpenAVNS
* _Openness_ – all source files, including code, PCB source files, CAD, and any documentation, available under permissive license
* _Accessibility_ – device developed using freely available tools (KiCad for PCBs, FreeCAD for enclosure, etc.); enclosure able to be 3D printed on home printer; PCBs use only common, off the shelf components and able to be taped out and assembled by a low-cost manufacturer; clear instructions are developed for building, flashing, and testing a device; low-cost, replaceable, commercially available electrodes are used to apply stimulation; apps should be developed on a platform that allows iOS and Android releases simultaneously
* _Wearability_ – device should be as small and unobtrusive as possible while maintaining openness and accessibility. Ideally, all electronics would fit behind the ear (similar to a behind-ear hearing aid or cochlear implant electronics) and can be comfortably worn for at least 30 minutes at a time
* _Effectiveness_ – the device should be capable of outputting stimulation with the same parameters as those used in clinical trials for at least 30 minutes. We note that OpenAVNS cannot be considered to be “effective” for any indication until it has been tested in a clinical trial.
* _Safety_ – the device’s electronics, firmware, software, and mechanical design should all be constructed with user safety in mind, and should be built to the standards of a medical device. We note that until OpenAVNS has been tested and cleared by a regulatory body for human use it cannot be considered “safe”; any use of the source files and documentation in the project must be done for educational purposes only and is done entirely at the user’s own risk. PLEASE DO NOT HURT YOURSELF!
 
## Proposed user needs and design inputs
| User Need | Design Input |
|-----------|--------------|
| The device shall apply neuromodulation to the auricular vagus branches at the tragus and cymba concha | Two biphasic outputs capable of current-driven stimulation |
| The user shall easily activate, modify, and deactivate stimulation on the device | App with clear, intuitive controls allowing for control of stimulation parameters <br><br> Physical button on device to deactivate stimulation if needed |
| The user shall comfortably wear the device | Weight less than 40g <br><br> Device fits behind ear <br><br> Contains features for holding device in place in the ear |
| The user shall apply neuromodulation typical of clinical studies reported in literature | Adjustable outputs between 1-50Hz, 100-500us pulse width, 0-2mA amplitude <br><br> Capable of outputting at maximum settings on one channel for a minimum of 30min |
| The device is open and accessible to a large range of users in different situations | Rechargeable by USB-C <br><br> Uses commercial off the shelf replaceable electrodes <br><br> App available on iOS and Android platforms |
| The device is safe to use | Device hardware prevents DC charge or amplitudes outside of the operating parameters from presenting at the electrode sites <br><br> Physical button on device to stop stimulation <br><br> Impedance measurement on output used to stop stimulation if electrodes are disconnected (short or open circuit) <br><br> Stimulation stops if wireless connection to app is interrupted <br><br> Firmware automatically stops stimulation after 30min regardless of other factors |

## Project Goals / Timeline
_2025_
* _August_ set up project structure, onboard contributors, assign tasks, generate block diagrams of system
* _September_ design and tape out first version of PCB, develop test firmware, generate proof of concept drawings / mock-ups for enclosure and user interface
* _October_ test and document initial PCB and firmware for power draw, output integrity, and safety, develop initial app prototype, combine board needs and enclosure drawings to create CAD model
* _November_ tape out second version of PCB designed for enclosure, conduct fit test, create alpha version of iOS and Android app
* _December_ first system tests of prototype device (HW + FW + Enclosure + App)

## Disclaimer
The contents of this document, the OpenAVNS repository on GitHub, and any other documents or communication released by the OpenAVNS project (the “Materials”) are subject to revision. No representation or warranty, express or implied, is provided in relation to the accuracy, correctness, completeness, or reliability of the information, opinions, or conclusions expressed in this repository.

The Materials are experimental in nature and should be used with prudence and appropriate caution. Any use is voluntary and at your own risk.

The Materials are made available “as is” by the contributors to this repository (collectively, “Providers”). Providers expressly disclaim all warranties, express or implied, arising in connection with the Materials, or arising out of a course of performance, dealing, or trade usage, including, without limitation, any warranties of title, noninfringement, merchantability, or fitness for a particular purpose.

Any user of the Materials agrees to forever indemnify, defend, and hold harmless Providers from and against, and to waive any and all claims against Providers for any and all claims, suits, demands, liability, loss or damage whatsoever, including attorneys' fees, whether direct or consequential, on account of any loss, injury, death or damage to any person or (including without limitation all agents and employees of Providers and all property owned by, leased to or used by either Providers) or on account of any loss or damages to business or reputations or privacy of any persons, arising in whole or in part in any way from use of the Materials or in any way connected therewith or in any way related thereto.

The Materials, any related materials, and all intellectual property therein, are owned by Providers.
