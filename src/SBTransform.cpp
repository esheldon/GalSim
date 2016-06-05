/* -*- c++ -*-
 * Copyright (c) 2012-2016 by the GalSim developers team on GitHub
 * https://github.com/GalSim-developers
 *
 * This file is part of GalSim: The modular galaxy image simulation toolkit.
 * https://github.com/GalSim-developers/GalSim
 *
 * GalSim is free software: redistribution and use in source and binary forms,
 * with or without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions, and the disclaimer given in the accompanying LICENSE
 *    file.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the disclaimer given in the documentation
 *    and/or other materials provided with the distribution.
 */

//#define DEBUGLOGGING

#include "TMV.h"
#include "SBTransform.h"
#include "SBTransformImpl.h"

#ifdef DEBUGLOGGING
#include <fstream>
//std::ostream* dbgout = new std::ofstream("debug.out");
//std::ostream* dbgout = &std::cout;
//int verbose_level = 1;
#endif

namespace galsim {

    SBTransform::SBTransform(const SBProfile& adaptee,
                             double mA, double mB, double mC, double mD,
                             const Position<double>& cen, double fluxScaling,
                             const GSParamsPtr& gsparams) :
        SBProfile(new SBTransformImpl(adaptee,mA,mB,mC,mD,cen,fluxScaling,gsparams)) {}

    SBTransform::SBTransform(const SBTransform& rhs) : SBProfile(rhs) {}

    SBTransform::~SBTransform() {}

    std::string SBTransform::SBTransformImpl::serialize() const
    {
        std::ostringstream oss(" ");
        oss.precision(std::numeric_limits<double>::digits10 + 4);
        oss << "galsim._galsim.SBTransform(" << getObj().serialize() << ", ";
        double mA, mB, mC, mD;
        getJac(mA,mB,mC,mD);
        oss << mA<<", "<<mB<<", "<<mC<<", "<<mD<<", ";
        Position<double> shift = getOffset();
        oss << "galsim.PositionD("<<shift.x<<", "<<shift.y<<"), "<<getFluxScaling();
        oss << ", galsim.GSParams("<<*gsparams<<"))";
        return oss.str();
    }

    SBProfile SBTransform::getObj() const
    {
        assert(dynamic_cast<const SBTransformImpl*>(_pimpl.get()));
        return static_cast<const SBTransformImpl&>(*_pimpl).getObj();
    }

    void SBTransform::getJac(double& mA, double& mB, double& mC, double& mD) const
    {
        assert(dynamic_cast<const SBTransformImpl*>(_pimpl.get()));
        return static_cast<const SBTransformImpl&>(*_pimpl).getJac(mA,mB,mC,mD);
    }

    Position<double> SBTransform::getOffset() const
    {
        assert(dynamic_cast<const SBTransformImpl*>(_pimpl.get()));
        return static_cast<const SBTransformImpl&>(*_pimpl).getOffset();
    }

    double SBTransform::getFluxScaling() const
    {
        assert(dynamic_cast<const SBTransformImpl*>(_pimpl.get()));
        return static_cast<const SBTransformImpl&>(*_pimpl).getFluxScaling();
    }


    SBTransform::SBTransformImpl::SBTransformImpl(
        const SBProfile& adaptee, double mA, double mB, double mC, double mD,
        const Position<double>& cen, double fluxScaling,
        const GSParamsPtr& gsparams) :
        SBProfileImpl(gsparams ? gsparams : GetImpl(adaptee)->gsparams),
        _adaptee(adaptee), _mA(mA), _mB(mB), _mC(mC), _mD(mD), _cen(cen), _fluxScaling(fluxScaling)
    {
        dbg<<"Start TransformImpl (1)\n";
        dbg<<"matrix = "<<_mA<<','<<_mB<<','<<_mC<<','<<_mD<<std::endl;
        dbg<<"cen = "<<_cen<<", fluxScaling = "<<_fluxScaling<<std::endl;

        // All the actual initialization is in a separate function so we can share code
        // with the other constructor.
        initialize();
    }

    void SBTransform::SBTransformImpl::initialize()
    {
        dbg<<"Start SBTransformImpl initialize\n";
        // First check if our adaptee is really another SBTransform:
        assert(GetImpl(_adaptee));
        const SBTransformImpl* sbd = dynamic_cast<const SBTransformImpl*>(GetImpl(_adaptee));
        dbg<<"sbd = "<<sbd<<std::endl;
        if (sbd) {
            dbg<<"wrapping another transformation.\n";
            // We are transforming something that's already a transformation.
            dbg<<"this transformation = "<<
                _mA<<','<<_mB<<','<<_mC<<','<<_mD<<','<<
                _cen<<','<<_fluxScaling<<std::endl;
            dbg<<"adaptee transformation = "<<
                sbd->_mA<<','<<sbd->_mB<<','<<sbd->_mC<<','<<sbd->_mD<<','<<
                sbd->_cen<<','<<sbd->_fluxScaling<<std::endl;
            dbg<<"adaptee getFlux = "<<_adaptee.getFlux()<<std::endl;
            // We are transforming something that's already a transformation.
            // So just compound the affine transformaions
            // New matrix is product (M_this) * (M_old)
            double mA = _mA; double mB=_mB; double mC=_mC; double mD=_mD;
            _cen += Position<double>(mA*sbd->_cen.x + mB*sbd->_cen.y,
                                     mC*sbd->_cen.x + mD*sbd->_cen.y);
            _mA = mA*sbd->_mA + mB*sbd->_mC;
            _mB = mA*sbd->_mB + mB*sbd->_mD;
            _mC = mC*sbd->_mA + mD*sbd->_mC;
            _mD = mC*sbd->_mB + mD*sbd->_mD;
            _fluxScaling *= sbd->_fluxScaling;
            dbg<<"this transformation => "<<
                _mA<<','<<_mB<<','<<_mC<<','<<_mD<<','<<
                _cen<<','<<_fluxScaling<<std::endl;
            _adaptee = sbd->_adaptee;
        } else {
            dbg<<"wrapping a non-transformation.\n";
            dbg<<"this transformation = "<<
                _mA<<','<<_mB<<','<<_mC<<','<<_mD<<','<<
                _cen<<','<<_fluxScaling<<std::endl;
        }

        // It will be reasonably common to have an identity matrix (for just
        // a flux scaling and/or shift) for (A,B,C,D).  If so, we can use simpler
        // versions of fwd and inv:
        if (_mA == 1. && _mB == 0. && _mC == 0. && _mD == 1.) {
            dbg<<"Using identity functions for fwd and inv\n";
            _fwd = &SBTransform::SBTransformImpl::_ident;
            _inv = &SBTransform::SBTransformImpl::_ident;
        } else {
            dbg<<"Using normal fwd and inv\n";
            _fwd = &SBTransform::SBTransformImpl::_fwd_normal;
            _inv = &SBTransform::SBTransformImpl::_inv_normal;
        }

        // Calculate some derived quantities:
        double det = _mA*_mD-_mB*_mC;
        if (det==0.) throw SBError("Attempt to SBTransform with degenerate matrix");
        _absdet = std::abs(det);
        _invdet = 1./det;
        _stillIsAxisymmetric = _adaptee.isAxisymmetric()
            && (_mB==-_mC)
            && (_mA==_mD)
            && (_cen.x==0.) && (_cen.y==0.); // Need pure rotation

        // Calculate maxK, stepK:
        double h1 = hypot( _mA+_mD, _mB-_mC);
        double h2 = hypot( _mA-_mD, _mB+_mC);
        double major = 0.5*std::abs(h1+h2);
        double minor = 0.5*std::abs(h1-h2);
        if (major < minor) std::swap(major,minor);
        _maxk = _adaptee.maxK() / minor;
        _stepk = _adaptee.stepK() / major;

        // If we have a shift, we need to further modify stepk
        //     stepk = Pi/R
        // R <- R + |shift|
        // stepk <- Pi/(Pi/stepk + |shift|)
        if (_cen.x != 0. || _cen.y != 0.) {
            double shift = sqrt( _cen.x*_cen.x + _cen.y*_cen.y );
            dbg<<"stepk from adaptee = "<<_stepk<<std::endl;
            _stepk = M_PI / (M_PI/_stepk + shift);
            dbg<<"shift = "<<shift<<", stepk -> "<<_stepk<<std::endl;
        }

        xdbg<<"Transformation init\n";
        xdbg<<"matrix = "<<_mA<<','<<_mB<<','<<_mC<<','<<_mD<<std::endl;
        xdbg<<"_cen = "<<_cen<<std::endl;
        xdbg<<"_invdet = "<<_invdet<<std::endl;
        xdbg<<"_absdet = "<<_absdet<<std::endl;
        xdbg<<"_fluxScaling = "<<_fluxScaling<<std::endl;
        xdbg<<"major, minor = "<<major<<", "<<minor<<std::endl;
        xdbg<<"maxK() = "<<_maxk<<std::endl;
        xdbg<<"stepK() = "<<_stepk<<std::endl;

        // Calculate the values for getXRange and getYRange:
        if (_adaptee.isAxisymmetric()) {
            // The original is a circle, so first get its radius.
            _adaptee.getXRange(_xmin,_xmax,_xsplits);
            if (_xmax == integ::MOCK_INF) {
                // Then these are correct, and use +- inf for y range too.
                _ymin = -integ::MOCK_INF;
                _ymax = integ::MOCK_INF;
            } else {
                double R = _xmax;
                // The transformation takes each point on the circle to the following new coordinates:
                // (x,y) -> (A*x + B*y + x0 , C*x + D*y + y0)
                // Using x = R cos(t) and y = R sin(t), we can find the minimum wrt t as:
                // xmax = R sqrt(A^2 + B^2) + x0
                // xmin = -R sqrt(A^2 + B^2) + x0
                // ymax = R sqrt(C^2 + D^2) + y0
                // ymin = -R sqrt(C^2 + D^2) + y0
                double AApBB = _mA*_mA + _mB*_mB;
                double sqrtAApBB = sqrt(AApBB);
                double temp = sqrtAApBB * R;
                _xmin = -temp + _cen.x;
                _xmax = temp + _cen.x;
                double CCpDD = _mC*_mC + _mD*_mD;
                double sqrtCCpDD = sqrt(CCpDD);
                temp = sqrt(CCpDD) * R;
                _ymin = -temp + _cen.y;
                _ymax = temp + _cen.y;
                _ysplits.resize(_xsplits.size());
                for (size_t k=0;k<_xsplits.size();++k) {
                    // The split points work the same way.  Scale them by the same factor we
                    // scaled the R value above, then add _cen.x or _cen.y.
                    double split = _xsplits[k];
                    xxdbg<<"Adaptee split at "<<split<<std::endl;
                    _xsplits[k] = sqrtAApBB * split + _cen.x;
                    _ysplits[k] = sqrtCCpDD * split + _cen.y;
                    xxdbg<<"-> x,y splits at "<<_xsplits[k]<<"  "<<_ysplits[k]<<std::endl;
                }
                // Now a couple of calculations that get reused in getYRangeX(x,yminymax):
                _coeff_b = (_mA*_mC + _mB*_mD) / AApBB;
                _coeff_c = CCpDD / AApBB;
                _coeff_c2 = _absdet*_absdet / AApBB;
                xxdbg<<"adaptee is axisymmetric.\n";
                xxdbg<<"adaptees maxR = "<<R<<std::endl;
                xxdbg<<"xmin..xmax = "<<_xmin<<" ... "<<_xmax<<std::endl;
                xxdbg<<"ymin..ymax = "<<_ymin<<" ... "<<_ymax<<std::endl;
            }
        } else {
            // Apply the transformation to each of the four corners of the original
            // and find the minimum and maximum.
            double xmin_1, xmax_1;
            std::vector<double> xsplits0;
            _adaptee.getXRange(xmin_1,xmax_1,xsplits0);
            double ymin_1, ymax_1;
            std::vector<double> ysplits0;
            _adaptee.getYRange(ymin_1,ymax_1,ysplits0);
            // Note: This doesn't explicitly check for MOCK_INF values.
            // It shouldn't be a problem, since the integrator will still treat
            // large values near MOCK_INF as infinity, but it just means that
            // the following calculations might be wasted flops.
            Position<double> bl = fwd(Position<double>(xmin_1,ymin_1));
            Position<double> br = fwd(Position<double>(xmax_1,ymin_1));
            Position<double> tl = fwd(Position<double>(xmin_1,ymax_1));
            Position<double> tr = fwd(Position<double>(xmax_1,ymax_1));
            _xmin = std::min(std::min(std::min(bl.x,br.x),tl.x),tr.x) + _cen.x;
            _xmax = std::max(std::max(std::max(bl.x,br.x),tl.x),tr.x) + _cen.x;
            _ymin = std::min(std::min(std::min(bl.y,br.y),tl.y),tr.y) + _cen.y;
            _ymax = std::max(std::max(std::max(bl.y,br.y),tl.y),tr.y) + _cen.y;
            xxdbg<<"adaptee is not axisymmetric.\n";
            xxdbg<<"adaptees x range = "<<xmin_1<<" ... "<<xmax_1<<std::endl;
            xxdbg<<"adaptees y range = "<<ymin_1<<" ... "<<ymax_1<<std::endl;
            xxdbg<<"Corners are: bl = "<<bl<<std::endl;
            xxdbg<<"             br = "<<br<<std::endl;
            xxdbg<<"             tl = "<<tl<<std::endl;
            xxdbg<<"             tr = "<<tr<<std::endl;
            xxdbg<<"xmin..xmax = "<<_xmin<<" ... "<<_xmax<<std::endl;
            xxdbg<<"ymin..ymax = "<<_ymin<<" ... "<<_ymax<<std::endl;
            if (bl.x + _cen.x > _xmin && bl.x + _cen.x < _xmax) {
                xxdbg<<"X Split from bl.x = "<<bl.x+_cen.x<<std::endl;
                _xsplits.push_back(bl.x+_cen.x);
            }
            if (br.x + _cen.x > _xmin && br.x + _cen.x < _xmax) {
                xxdbg<<"X Split from br.x = "<<br.x+_cen.x<<std::endl;
                _xsplits.push_back(br.x+_cen.x);
            }
            if (tl.x + _cen.x > _xmin && tl.x + _cen.x < _xmax) {
                xxdbg<<"X Split from tl.x = "<<tl.x+_cen.x<<std::endl;
                _xsplits.push_back(tl.x+_cen.x);
            }
            if (tr.x + _cen.x > _xmin && tr.x + _cen.x < _xmax) {
                xxdbg<<"X Split from tr.x = "<<tr.x+_cen.x<<std::endl;
                _xsplits.push_back(tr.x+_cen.x);
            }
            if (bl.y + _cen.y > _ymin && bl.y + _cen.y < _ymax) {
                xxdbg<<"Y Split from bl.y = "<<bl.y+_cen.y<<std::endl;
                _ysplits.push_back(bl.y+_cen.y);
            }
            if (br.y + _cen.y > _ymin && br.y + _cen.y < _ymax) {
                xxdbg<<"Y Split from br.y = "<<br.y+_cen.y<<std::endl;
                _ysplits.push_back(br.y+_cen.y);
            }
            if (tl.y + _cen.y > _ymin && tl.y + _cen.y < _ymax) {
                xxdbg<<"Y Split from tl.y = "<<tl.y+_cen.y<<std::endl;
                _ysplits.push_back(tl.y+_cen.y);
            }
            if (tr.y + _cen.y > _ymin && tr.y + _cen.y < _ymax) {
                xxdbg<<"Y Split from tr.y = "<<tr.y+_cen.y<<std::endl;
                _ysplits.push_back(tr.y+_cen.y);
            }
            // If the adaptee has any splits, try to propagate those up
            for(size_t k=0;k<xsplits0.size();++k) {
                xxdbg<<"Adaptee xsplit at "<<xsplits0[k]<<std::endl;
                Position<double> bx = fwd(Position<double>(xsplits0[k],ymin_1));
                Position<double> tx = fwd(Position<double>(xsplits0[k],ymax_1));
                if (bx.x + _cen.x > _xmin && bx.x + _cen.x < _xmax) {
                    xxdbg<<"X Split from bx.x = "<<bx.x+_cen.x<<std::endl;
                    _xsplits.push_back(bx.x+_cen.x);
                }
                if (tx.x + _cen.x > _xmin && tx.x + _cen.x < _xmax) {
                    xxdbg<<"X Split from tx.x = "<<tx.x+_cen.x<<std::endl;
                    _xsplits.push_back(tx.x+_cen.x);
                }
                if (bx.y + _cen.y > _ymin && bx.y + _cen.y < _ymax) {
                    xxdbg<<"Y Split from bx.y = "<<bx.y+_cen.y<<std::endl;
                    _ysplits.push_back(bx.y+_cen.y);
                }
                if (tx.y + _cen.y > _ymin && tx.y + _cen.y < _ymax) {
                    xxdbg<<"Y Split from tx.y = "<<tx.y+_cen.y<<std::endl;
                    _ysplits.push_back(tx.y+_cen.y);
                }
            }
            for(size_t k=0;k<ysplits0.size();++k) {
                xxdbg<<"Adaptee ysplit at "<<ysplits0[k]<<std::endl;
                Position<double> yl = fwd(Position<double>(xmin_1,ysplits0[k]));
                Position<double> yr = fwd(Position<double>(xmax_1,ysplits0[k]));
                if (yl.x + _cen.x > _xmin && yl.x + _cen.x < _xmax) {
                    xxdbg<<"X Split from tl.x = "<<tl.x+_cen.x<<std::endl;
                    _xsplits.push_back(yl.x+_cen.x);
                }
                if (yr.x + _cen.x > _xmin && yr.x + _cen.x < _xmax) {
                    xxdbg<<"X Split from yr.x = "<<yr.x+_cen.x<<std::endl;
                    _xsplits.push_back(yr.x+_cen.x);
                }
                if (yl.y + _cen.y > _ymin && yl.y + _cen.y < _ymax) {
                    xxdbg<<"Y Split from yl.y = "<<yl.y+_cen.y<<std::endl;
                    _ysplits.push_back(yl.y+_cen.y);
                }
                if (yr.y + _cen.y > _ymin && yr.y + _cen.y < _ymax) {
                    xxdbg<<"Y Split from yr.y = "<<yr.y+_cen.y<<std::endl;
                    _ysplits.push_back(yr.y+_cen.y);
                }
            }
        }
        // At this point we are done with _absdet per se.  Multiply it by _fluxScaling
        // so we can use it as the scale factor for kValue and getFlux.
        _absdet *= _fluxScaling;
        xdbg<<"_absdet -> "<<_absdet<<std::endl;

        // Figure out which function we need for kValue and kValueNoPhase
        if (std::abs(_absdet-1.) < this->gsparams->kvalue_accuracy) {
            xdbg<<"absdet*fluxScaling = "<<_absdet<<" = 1, so use NoDet version.\n";
            _kValueNoPhase = &SBTransform::SBTransformImpl::_kValueNoPhaseNoDet;
        } else {
            xdbg<<"absdet*fluxScaling = "<<_absdet<<" != 1, so use WithDet version.\n";
            _kValueNoPhase = &SBTransform::SBTransformImpl::_kValueNoPhaseWithDet;
        }
        if (_cen.x == 0. && _cen.y == 0.) {
            _zeroCen = true;
            _kValue = _kValueNoPhase;
        } else {
            _zeroCen = false;
            _kValue = &SBTransform::SBTransformImpl::_kValueWithPhase;
        }
    }

    void SBTransform::SBTransformImpl::getXRange(
        double& xmin, double& xmax, std::vector<double>& splits) const
    {
        xmin = _xmin; xmax = _xmax;
        splits.insert(splits.end(),_xsplits.begin(),_xsplits.end());
    }

    void SBTransform::SBTransformImpl::getYRange(
        double& ymin, double& ymax, std::vector<double>& splits) const
    {
        ymin = _ymin; ymax = _ymax;
        splits.insert(splits.end(),_ysplits.begin(),_ysplits.end());
    }

    void SBTransform::SBTransformImpl::getYRangeX(
        double x, double& ymin, double& ymax, std::vector<double>& splits) const
    {
        xxdbg<<"Transformation getYRangeX for x = "<<x<<std::endl;
        if (_adaptee.isAxisymmetric()) {
            std::vector<double> splits0;
            _adaptee.getYRange(ymin,ymax,splits0);
            if (ymax == integ::MOCK_INF) return;
            double R = ymax;
            // The circlue with radius R is mapped onto an ellipse with (x,y) given by:
            // x = A R cos(t) + B R sin(t) + x0
            // y = C R cos(t) + D R sin(t) + y0
            //
            // Or equivalently:
            // (A^2+B^2) (y-y0)^2 - 2(AC+BD) (x-x0)(y-y0) + (C^2+D^2) (x-x0)^2 = R^2 (AD-BC)^2
            //
            // Given a particular value for x, we solve the latter equation for the
            // corresponding range for y.
            // y^2 - 2 b y = c
            // -> y^2 - 2b y = c
            //    (y - b)^2 = c + b^2
            //    y = b +- sqrt(c + b^2)
            double b = _coeff_b * (x-_cen.x);
            double c = _coeff_c2 * R*R - _coeff_c * (x-_cen.x) * (x-_cen.x);
            double d = sqrt(c + b*b);
            ymax = b + d + _cen.y;
            ymin = b - d + _cen.y;
            for (size_t k=0;k<splits0.size();++k) if (splits0[k] >= 0.) {
                double r = splits0[k];
                double c = _coeff_c2 * r*r - _coeff_c * (x-_cen.x) * (x-_cen.x);
                double d = sqrt(c+b*b);
                splits.push_back(b + d + _cen.y);
                splits.push_back(b - d + _cen.y);
            }
            xxdbg<<"Axisymmetric adaptee with R = "<<R<<std::endl;
            xxdbg<<"ymin .. ymax = "<<ymin<<" ... "<<ymax<<std::endl;
        } else {
            // There are 4 lines to check for where they intersect the given x.
            // Start with the adaptee's given ymin.
            // This line is transformed onto the line:
            // (x',ymin) -> ( A x' + B ymin + x0 , C x' + D ymin + y0 )
            // x' = (x - x0 - B ymin) / A
            // y = C x' + D ymin + y0
            //   = C (x - x0 - B ymin) / A + D ymin + y0
            // The top line is analagous for ymax instead of ymin.
            //
            // The left line is transformed as:
            // (xmin,y) -> ( A xmin + B y' + x0 , C xmin + D y' + y0 )
            // y' = (x - x0 - A xmin) / B
            // y = C xmin + D (x - x0 - A xmin) / B + y0
            // And again, the right line is analgous.
            //
            // We also need to check for A or B = 0, since then only one pair of lines is
            // relevant.
            xxdbg<<"Non-axisymmetric adaptee\n";
            if (_mA == 0.) {
                xxdbg<<"_mA == 0:\n";
                double xmin_1, xmax_1;
                std::vector<double> xsplits0;
                _adaptee.getXRange(xmin_1,xmax_1,xsplits0);
                xxdbg<<"xmin_1, xmax_1 = "<<xmin_1<<','<<xmax_1<<std::endl;
                ymin = _mC * xmin_1 + _mD * (x - _cen.x - _mA*xmin_1) / _mB + _cen.y;
                ymax = _mC * xmax_1 + _mD * (x - _cen.x - _mA*xmax_1) / _mB + _cen.y;
                if (ymax < ymin) std::swap(ymin,ymax);
                for(size_t k=0;k<xsplits0.size();++k) {
                    double xx = xsplits0[k];
                    splits.push_back(_mC * xx + _mD * (x - _cen.x - _mA*xx) / _mB + _cen.y);
                }
            } else if (_mB == 0.) {
                xxdbg<<"_mB == 0:\n";
                double ymin_1, ymax_1;
                std::vector<double> ysplits0;
                _adaptee.getYRange(ymin_1,ymax_1,ysplits0);
                xxdbg<<"ymin_1, ymax_1 = "<<ymin_1<<','<<ymax_1<<std::endl;
                ymin = _mC * (x - _cen.x - _mB*ymin_1) / _mA + _mD*ymin_1 + _cen.y;
                ymax = _mC * (x - _cen.x - _mB*ymax_1) / _mA + _mD*ymax_1 + _cen.y;
                if (ymax < ymin) std::swap(ymin,ymax);
                for(size_t k=0;k<ysplits0.size();++k) {
                    double yy = ysplits0[k];
                    splits.push_back(_mC * (x - _cen.x - _mB*yy) / _mA + _mD*yy + _cen.y);
                }
            } else {
                xxdbg<<"_mA,B != 0:\n";
                double ymin_1, ymax_1;
                std::vector<double> xsplits0;
                _adaptee.getYRange(ymin_1,ymax_1,xsplits0);
                xxdbg<<"ymin_1, ymax_1 = "<<ymin_1<<','<<ymax_1<<std::endl;
                ymin = _mC * (x - _cen.x - _mB*ymin_1) / _mA + _mD*ymin_1 + _cen.y;
                ymax = _mC * (x - _cen.x - _mB*ymax_1) / _mA + _mD*ymax_1 + _cen.y;
                xxdbg<<"From top and bottom: ymin,ymax = "<<ymin<<','<<ymax<<std::endl;
                if (ymax < ymin) std::swap(ymin,ymax);
                double xmin_1, xmax_1;
                std::vector<double> ysplits0;
                _adaptee.getXRange(xmin_1,xmax_1,ysplits0);
                xxdbg<<"xmin_1, xmax_1 = "<<xmin_1<<','<<xmax_1<<std::endl;
                ymin_1 = _mC * xmin_1 + _mD * (x - _cen.x - _mA*xmin_1) / _mB + _cen.y;
                ymax_1 = _mC * xmax_1 + _mD * (x - _cen.x - _mA*xmax_1) / _mB + _cen.y;
                xxdbg<<"From left and right: ymin,ymax = "<<ymin_1<<','<<ymax_1<<std::endl;
                if (ymax_1 < ymin_1) std::swap(ymin_1,ymax_1);
                if (ymin_1 > ymin) ymin = ymin_1;
                if (ymax_1 < ymax) ymax = ymax_1;
                for(size_t k=0;k<ysplits0.size();++k) {
                    double yy = ysplits0[k];
                    splits.push_back(_mC * (x - _cen.x - _mB*yy) / _mA + _mD*yy + _cen.y);
                }
                for(size_t k=0;k<xsplits0.size();++k) {
                    double xx = xsplits0[k];
                    splits.push_back(_mC * xx + _mD * (x - _cen.x - _mA*xx) / _mB + _cen.y);
                }
            }
            xxdbg<<"ymin .. ymax = "<<ymin<<" ... "<<ymax<<std::endl;
        }
    }

    double SBTransform::SBTransformImpl::xValue(const Position<double>& p) const
    { return _adaptee.xValue(inv(p-_cen)) * _fluxScaling; }

    std::complex<double> SBTransform::SBTransformImpl::kValue(const Position<double>& k) const
    { return _kValue(_adaptee,fwdT(k),_absdet,k,_cen); }

    std::complex<double> SBTransform::SBTransformImpl::kValueNoPhase(
        const Position<double>& k) const
    { return _kValueNoPhase(_adaptee,fwdT(k),_absdet,k,_cen); }

    std::complex<double> SBTransform::SBTransformImpl::_kValueNoPhaseNoDet(
        const SBProfile& adaptee, const Position<double>& fwdTk, double absdet,
        const Position<double>& , const Position<double>& )
    { return adaptee.kValue(fwdTk); }

    std::complex<double> SBTransform::SBTransformImpl::_kValueNoPhaseWithDet(
        const SBProfile& adaptee, const Position<double>& fwdTk, double absdet,
        const Position<double>& , const Position<double>& )
    { return absdet * adaptee.kValue(fwdTk); }

    std::complex<double> SBTransform::SBTransformImpl::_kValueWithPhase(
        const SBProfile& adaptee, const Position<double>& fwdTk, double absdet,
        const Position<double>& k, const Position<double>& cen)
    { return adaptee.kValue(fwdTk) * std::polar(absdet , -k.x*cen.x-k.y*cen.y); }

    void SBTransform::SBTransformImpl::fillXValue(tmv::MatrixView<double> val,
                                                  double x0, double dx, int izero,
                                                  double y0, double dy, int jzero) const
    {
        dbg<<"SBTransform fillXValue\n";
        dbg<<"x = "<<x0<<" + i * "<<dx<<", izero = "<<izero<<std::endl;
        dbg<<"y = "<<y0<<" + j * "<<dy<<", jzero = "<<jzero<<std::endl;
        dbg<<"A,B,C,D = "<<_mA<<','<<_mB<<','<<_mC<<','<<_mD<<std::endl;
        dbg<<"cen = "<<_cen<<", zerocen = "<<_zeroCen<<std::endl;
        dbg<<"absdet = "<<_absdet<<", invdet = "<<_invdet<<std::endl;
        dbg<<"fluxScaling = "<<_fluxScaling<<std::endl;

        // Subtract cen
        if (!_zeroCen) {
            x0 -= _cen.x;
            y0 -= _cen.y;
            // Check if the new center falls on an integer index.
            // 0 = x0 + iz * dx
            // 0 = y0 + jz * dy
            xdbg<<"x0,y0 = "<<x0<<','<<y0<<std::endl;
            int iz = int(-x0/dx+0.5);
            int jz = int(-y0/dy+0.5);
            xdbg<<"iz,jz = "<<iz<<','<<jz<<std::endl;
            xdbg<<"near zero at "<<(x0+iz*dx)<<"  "<<(y0+jz*dy)<<std::endl;

            if (std::abs(x0 + iz*dx) < 1.e-10 && iz > 0 && iz < int(val.colsize())) izero = iz;
            else izero = 0;
            if (std::abs(y0 + jz*dy) < 1.e-10 && jz > 0 && jz < int(val.rowsize())) jzero = jz;
            else jzero = 0;
        }

        // Apply inv to x,y
        if (_mB == 0. && _mC == 0.) {
            double xscal = _invdet * _mD;
            double yscal = _invdet * _mA;
            x0 *= xscal;
            dx *= xscal;
            y0 *= yscal;
            dy *= yscal;

            GetImpl(_adaptee)->fillXValue(val,x0,dx,izero,y0,dy,jzero);
        } else {
            Position<double> inv0 = inv(Position<double>(x0,y0));
            Position<double> inv1 = inv(Position<double>(dx,0.));
            Position<double> inv2 = inv(Position<double>(0.,dy));
            xdbg<<"inv0 = "<<inv0<<std::endl;
            xdbg<<"inv1 = "<<inv1<<std::endl;
            xdbg<<"inv2 = "<<inv2<<std::endl;

            GetImpl(_adaptee)->fillXValue(val,inv0.x,inv1.x,inv2.x,inv0.y,inv2.y,inv1.y);
        }

        // Apply flux scaling
        val *= _fluxScaling;
    }

    void SBTransform::SBTransformImpl::fillKValue(tmv::MatrixView<std::complex<double> > val,
                                                  double kx0, double dkx, int izero,
                                                  double ky0, double dky, int jzero) const
    {
        dbg<<"SBTransform fillKValue\n";
        dbg<<"kx = "<<kx0<<" + i * "<<dkx<<", izero = "<<izero<<std::endl;
        dbg<<"ky = "<<ky0<<" + j * "<<dky<<", jzero = "<<jzero<<std::endl;
        dbg<<"A,B,C,D = "<<_mA<<','<<_mB<<','<<_mC<<','<<_mD<<std::endl;
        dbg<<"cen = "<<_cen<<", zerocen = "<<_zeroCen<<std::endl;
        dbg<<"absdet = "<<_absdet<<", invdet = "<<_invdet<<std::endl;
        dbg<<"fluxScaling = "<<_fluxScaling<<std::endl;

        // Apply fwdT to kx,ky
        if (_mB == 0. && _mC == 0.) {
            double fwdT_kx0 = _mA * kx0;
            double fwdT_dkx = _mA * dkx;
            double fwdT_ky0 = _mD * ky0;
            double fwdT_dky = _mD * dky;

            GetImpl(_adaptee)->fillKValue(val,fwdT_kx0,fwdT_dkx,izero,fwdT_ky0,fwdT_dky,jzero);
        } else {
            Position<double> fwdT0 = fwdT(Position<double>(kx0,ky0));
            Position<double> fwdT1 = fwdT(Position<double>(dkx,0.));
            Position<double> fwdT2 = fwdT(Position<double>(0.,dky));
            xdbg<<"fwdT0 = "<<fwdT0<<std::endl;
            xdbg<<"fwdT1 = "<<fwdT1<<std::endl;
            xdbg<<"fwdT2 = "<<fwdT2<<std::endl;

            GetImpl(_adaptee)->fillKValue(val,fwdT0.x,fwdT1.x,fwdT2.x,fwdT0.y,fwdT2.y,fwdT1.y);
        }

        // Apply phases
        if (_zeroCen) {
            xdbg<<"zeroCen\n";
            val *= _absdet;
        } else {
            xdbg<<"!zeroCen\n";
            // Make phase terms = |det| exp(-i(kx*cenx + ky*ceny))
            // In this case, the terms are separable, so only need to make kx and ky phases
            // separately.
            const int m = val.colsize();
            const int n = val.rowsize();
            tmv::Vector<std::complex<double> > kx_phase(m);
            tmv::Vector<std::complex<double> > ky_phase(n);
            typedef tmv::VIt<std::complex<double>,1,tmv::NonConj> CIt;
            CIt kx_phit = kx_phase.begin();
            CIt ky_phit = ky_phase.begin();
            for (int i=0;i<m;++i,kx0+=dkx) *kx_phit++ = std::polar(_absdet, -kx0*_cen.x);
            // Only use _absdet on one of them!
            for (int j=0;j<n;++j,ky0+=dky) *ky_phit++ = std::polar(1., -ky0*_cen.y);

            val = DiagMatrixViewOf(kx_phase) * val;
            val = val * DiagMatrixViewOf(ky_phase);
        }
    }

    void SBTransform::SBTransformImpl::fillXValue(tmv::MatrixView<double> val,
                                                  double x0, double dx, double dxy,
                                                  double y0, double dy, double dyx) const
    {
        dbg<<"SBTransform fillXValue\n";
        dbg<<"x = "<<x0<<" + i * "<<dx<<" + j * "<<dxy<<std::endl;
        dbg<<"y = "<<y0<<" + i * "<<dyx<<" + j * "<<dy<<std::endl;
        dbg<<"A,B,C,D = "<<_mA<<','<<_mB<<','<<_mC<<','<<_mD<<std::endl;
        dbg<<"cen = "<<_cen<<", zerocen = "<<_zeroCen<<std::endl;
        dbg<<"absdet = "<<_absdet<<", invdet = "<<_invdet<<std::endl;
        dbg<<"fluxScaling = "<<_fluxScaling<<std::endl;

        // Subtract cen
        if (!_zeroCen) {
            x0 -= _cen.x;
            y0 -= _cen.y;
        }

        // Apply inv to x,y
        Position<double> inv0 = inv(Position<double>(x0,y0));
        Position<double> inv1 = inv(Position<double>(dx,dyx));
        Position<double> inv2 = inv(Position<double>(dxy,dy));
        xdbg<<"inv0 = "<<inv0<<std::endl;
        xdbg<<"inv1 = "<<inv1<<std::endl;
        xdbg<<"inv2 = "<<inv2<<std::endl;

        GetImpl(_adaptee)->fillXValue(val,inv0.x,inv1.x,inv2.x,inv0.y,inv2.y,inv1.y);

        // Apply flux scaling
        val *= _fluxScaling;
    }

    void SBTransform::SBTransformImpl::fillKValue(tmv::MatrixView<std::complex<double> > val,
                                                  double kx0, double dkx, double dkxy,
                                                  double ky0, double dky, double dkyx) const
    {
        dbg<<"SBTransform fillKValue\n";
        dbg<<"kx = "<<kx0<<" + i * "<<dkx<<" + j * "<<dkxy<<std::endl;
        dbg<<"ky = "<<ky0<<" + i * "<<dkyx<<" + j * "<<dky<<std::endl;
        dbg<<"A,B,C,D = "<<_mA<<','<<_mB<<','<<_mC<<','<<_mD<<std::endl;
        dbg<<"cen = "<<_cen<<", zerocen = "<<_zeroCen<<std::endl;
        dbg<<"absdet = "<<_absdet<<", invdet = "<<_invdet<<std::endl;
        dbg<<"fluxScaling = "<<_fluxScaling<<std::endl;

        // Apply fwdT to kx,ky
        // Original (x,y):
        //     kx = kx0 + i dkx + j dkxy
        //     ky = ky0 + i dkyx + j dky
        // (kx',ky') = fwdT(kx,ky)
        //     kx' = A kx + C ky
        //         = (A kx0 + C ky0) + i (A dkx + C dkyx) + j (A dkxy + C dky)
        //     ky' = B kx + D ky
        //         = (B kx0 + D ky0) + i (B dkx + D dkyx) + j (B dkxy + D dky)
        //
        Position<double> fwdT0 = fwdT(Position<double>(kx0,ky0));
        Position<double> fwdT1 = fwdT(Position<double>(dkx,dkyx));
        Position<double> fwdT2 = fwdT(Position<double>(dkxy,dky));
        xdbg<<"fwdT0 = "<<fwdT0<<std::endl;
        xdbg<<"fwdT1 = "<<fwdT1<<std::endl;
        xdbg<<"fwdT2 = "<<fwdT2<<std::endl;

        GetImpl(_adaptee)->fillKValue(val,fwdT0.x,fwdT1.x,fwdT2.x,fwdT0.y,fwdT2.y,fwdT1.y);

        // Apply phase terms = |det| exp(-i(kx*cenx + ky*ceny))
        if (_zeroCen) {
            xdbg<<"zeroCen\n";
            val *= _absdet;
        } else {
            xdbg<<"!zeroCen\n";
            assert(val.stepi() == 1);
            assert(val.canLinearize());
            const int m = val.colsize();
            const int n = val.rowsize();

            typedef tmv::VIt<std::complex<double>,1,tmv::NonConj> It;
            It valit = val.linearView().begin();
            kx0 *= _cen.x;
            dkx *= _cen.x;
            dkxy *= _cen.x;
            ky0 *= _cen.y;
            dky *= _cen.y;
            dkyx *= _cen.y;
            for (int j=0;j<n;++j,kx0+=dkxy,ky0+=dky) {
                double kx = kx0;
                double ky = ky0;
                for (int i=0;i<m;++i,kx+=dkx,ky+=dkyx) *valit++ *= std::polar(_absdet, -kx-ky);
            }
        }
    }

    boost::shared_ptr<PhotonArray> SBTransform::SBTransformImpl::shoot(
        int N, UniformDeviate u) const
    {
        dbg<<"Distort shoot: N = "<<N<<std::endl;
        dbg<<"Target flux = "<<getFlux()<<std::endl;
        // Simple job here: just remap coords of each photon, then change flux
        // If there is overall magnification in the transform
        boost::shared_ptr<PhotonArray> result = _adaptee.shoot(N,u);
        for (int i=0; i<result->size(); i++) {
            Position<double> xy = fwd(Position<double>(result->getX(i), result->getY(i)))+_cen;
            result->setPhoton(i,xy.x, xy.y, result->getFlux(i)*_absdet);
        }
        dbg<<"Distort Realized flux = "<<result->getTotalFlux()<<std::endl;
        return result;
    }
}
