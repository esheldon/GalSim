# Copyright (c) 2012-2016 by the GalSim developers team on GitHub
# https://github.com/GalSim-developers
#
# This file is part of GalSim: The modular galaxy image simulation toolkit.
# https://github.com/GalSim-developers/GalSim
#
# GalSim is free software: redistribution and use in source and binary forms,
# with or without modification, are permitted provided that the following
# conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions, and the disclaimer given in the accompanying LICENSE
#    file.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions, and the disclaimer given in the documentation
#    and/or other materials provided with the distribution.
#

import galsim
from galsim.deprecated import depr

def SED_rdiv(self, other):
    depr('__rdiv__', 1.3, "SED(spec=lambda wave:other/bandpass(wave))",
            "We removed this function because we don't know of any clear use case. "+
            "If you have one, please open an issue, and we can add this function back.")
    if hasattr(other, '__call__'):
        wave_factor = 1.0 + self.redshift
        spec = lambda w: other(w * wave_factor) /  self._rest_photons(w)
    else:
        spec = lambda w: other / self._rest_photons(w)
    return galsim.SED(spec, wave_type='nm', flux_type='fphotons',
                      redshift=self.redshift, _wave_list=self.wave_list,
                      _blue_limit=self.blue_limit, _red_limit=self.red_limit)

galsim.SED.__rdiv__ = SED_rdiv
galsim.SED.__rtruediv__ = SED_rdiv
