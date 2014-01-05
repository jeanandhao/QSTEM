/*
  QSTEM - image simulation for TEM/STEM/CBED
  Copyright (C) 2000-2010  Christoph Koch
  Copyright (C) 2010-2013  Christoph Koch, Michael Sarahan
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "experiment_interface.hpp"
#include "potential.hpp"
#include "wavefunctions.hpp"

class CExperimentBase : public IExperiment
{
public:
  virtual void DisplayProgress(int flag, MULS &muls, WavePtr &wave, StructurePtr &crystal);
  virtual void SaveImages()=0;
  virtual void run()=0;
protected:
  WavePtr m_wave;
  PotPtr m_pot;
}
