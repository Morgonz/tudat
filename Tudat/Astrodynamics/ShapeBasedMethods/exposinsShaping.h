/*    Copyright (c) 2010-2018, Delft University of Technology
 *    All rigths reserved
 *
 *    This file is part of the Tudat. Redistribution and use in source and
 *    binary forms, with or without modification, are permitted exclusively
 *    under the terms of the Modified BSD license. You should have received
 *    a copy of the license with this file. If not, please or visit:
 *    http://tudat.tudelft.nl/LICENSE.
 *
 */

#ifndef EXPOSINSSHAPING_H
#define EXPOSINSSHAPING_H

#include "Tudat/Astrodynamics/ShapeBasedMethods/shapeBasedMethodLeg.h"
//#include "Tudat/Astrodynamics/ShapeBasedMethods/baseFunctionsExposinsShaping.h"
#include "Tudat/Astrodynamics/ShapeBasedMethods/compositeFunctionExposinsShaping.h"
#include "Tudat/Mathematics/NumericalIntegrators/createNumericalIntegrator.h"
#include "Tudat/Mathematics/NumericalQuadrature/createNumericalQuadrature.h"
#include <Tudat/SimulationSetup/tudatSimulationHeader.h>
#include <math.h>
#include <vector>
#include <Eigen/Dense>
#include <map>

namespace tudat
{
namespace shape_based_methods
{


class ExposinsShaping : public ShapeBasedMethodLeg  /*: public ShapeBasedMethod*/
{
public:

    //! Constructor for exposins shaping.
    ExposinsShaping(Eigen::Vector6d initialState,
                     Eigen::Vector6d finalState,
                     double requiredTimeOfFlight,
                     int numberOfRevolutions,
                     simulation_setup::NamedBodyMap& bodyMap,
                     const std::string bodyToPropagate,
                     const std::string centralBody,
                     double windingParameter,
                     std::shared_ptr< root_finders::RootFinderSettings >& rootFinderSettings,
//                     const double lowerBoundFreeCoefficient = TUDAT_NAN,
//                     const double upperBoundFreeCoefficient = TUDAT_NAN,
                     std::shared_ptr< numerical_integrators::IntegratorSettings< double > > integratorSettings
                        = std::shared_ptr< numerical_integrators::IntegratorSettings< > >( ) );

    //! Default destructor.
    ~ExposinsShaping( ) { }

    //! Convert time to independent variable.
    double convertTimeToIndependentVariable( const double time );

    //! Convert independent variable to time.
    double convertIndependentVariableToTime( const double independentVariable );

    //! Returns initial value of the independent variable.
    double getInitialValueInpendentVariable( )
    {
        return initialAzimuthAngle_;
    }

    //! Returns final value of the independent variable.
    double getFinalValueInpendentVariable( )
    {
        return finalAzimuthAngle_;
    }

    //! Return the coefficients of the radial distance composite function.
    Eigen::VectorXd getRadialDistanceFunctionCoefficients( )
    {
        return coefficientsRadialDistanceFunction_;
    }

    //! Return initial azimuth angle.
    double getInitialAzimuthAngle( )
    {
        return initialAzimuthAngle_;
    }

    //! Return final azimuth angle.
    double getFinalAzimuthAngle( )
    {
        return finalAzimuthAngle_;
    }

    //! Return wheter trajectory is feasible.
    bool getInfeasibleTOF( )
    {
        return infeasibleTOF_;
    }

    //! Return final azimuth angle.
    double getTravelledAzimuthAngle( )
    {
        return travelledAzimuthAngle_;
    }

    //! Return required Gamma.
    double getRequiredGamma( )
    {
        return requiredGamma_;
    }

    //! Compute current time from azimuth angle.
    double computeCurrentTimeFromAzimuthAngle( const double currentAzimuthAngle );

    //! Compute time of flight.
    double computeTimeOfFlight()
    {
        return computeNormalizedTimeOfFlight() * physical_constants::JULIAN_YEAR;
    }


    //! Compute current cartesian state.
    Eigen::Vector6d computeCurrentStateVector( const double currentAzimuthAngle );


    //! Compute current thrust acceleration in cartesian coordinates.
    Eigen::Vector3d computeCurrentThrustAccelerationVector( const double currentAzimuthAngle )
    {
        return computeNormalizedThrustAccelerationVector( currentAzimuthAngle ) * physical_constants::ASTRONOMICAL_UNIT
                / std::pow( physical_constants::JULIAN_YEAR, 2.0 );
    }

    //! Compute first derivative of the azimuth angle w.r.t. time. - Exposins
    double computeFirstDerivativeAzimuthAngleWrtTime( const double currentAzimuthAngle );

    //! Compute deltaV.
    double computeDeltaV( );

    //! Compute deltaV.
    double computeDeltaVHohmann( double centralBodyGravitationalParameter );

    //! Compute deltaV.
    double computeDeltaVBoundaries( );

    //! Compute normalized magnitude thrust acceleration. Exposins
    double computeCurrentNormalizedThrustAccelerationMagnitude( double currentAzimuthAngle );

    //! Compute magnitude thrust acceleration.
    double computeCurrentThrustAccelerationMagnitude( const double currentAzimuthAngle );

//    //! Get low-thrust acceleration model from shaping method.
//    std::shared_ptr< propulsion::ThrustAcceleration > getLowThrustAccelerationModel(
////            simulation_setup::NamedBodyMap& bodyMap,
////            const std::string& bodyToPropagate,
//            std::function< double( const double ) > specificImpulseFunction,
//            std::shared_ptr< interpolators::OneDimensionalInterpolator< double, double > > interpolatorPolarAngleFromTime );

    //! Function to compute the shaped trajectory and the propagation fo the full problem.
    void computeShapedTrajectoryAndFullPropagation(
//            simulation_setup::NamedBodyMap& bodyMap,
            std::function< double ( const double ) > specificImpulseFunction,
            const std::shared_ptr<numerical_integrators::IntegratorSettings<double> > integratorSettings,
            std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
                    std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > >& propagatorSettings,
            std::map< double, Eigen::VectorXd >& fullPropagationResults,
            std::map< double, Eigen::VectorXd >& shapingMethodResults,
            std::map< double, Eigen::VectorXd>& dependentVariablesHistory,
            const bool isMassPropagated );



protected:

    //! Compute the bounds of the gamma value.
    void computeBoundsGamma( );


    //! Iterate to match the required time of flight, by updating the value of the free coefficient.
    void iterateToMatchRequiredTimeOfFlight( std::shared_ptr< root_finders::RootFinderSettings > rootFinderSettings,
                                             const double lowerBound = TUDAT_NAN,
                                             const double upperBound = TUDAT_NAN,
                                             const double initialGuess = TUDAT_NAN );

    //! Compute Base time of flight function w.r.t. azimuth angle. - exposins
    double computeBaseTimeOfFlightFunction( double currentAzimuthAngle );

    //! Return the required value for the time of flight (normalized w.r.t. julian year).
    double getNormalizedRequiredTimeOfFlight( )
    {
        return requiredTimeOfFlight_;
    }

    //! Compute normalized time of flight.
    double computeNormalizedTimeOfFlight();


    //! Reset the value of the free coefficient, in order to match the required time of flight.
    void resetRequiredGamma( const double requiredGamma )
    {
        requiredGamma_ = requiredGamma;
    }

    //! Update shape equation coefficients
    void updateShapeCoefficients( );

    //! Computed travelled angle theta_f
    void computeTravelledAzimuthAngle( );

    //! Compute second derivative of the azimuth angle w.r.t. time.
    double computeSecondDerivativeAzimuthAngleWrtTime( const double currentAzimuthAngle );

    //! Compute current velocity in spherical coordinates parametrized by azimuth angle theta.
    Eigen::Vector3d computeCurrentVelocityParametrizedByAzimuthAngle( const double currentAzimuthAngle );

    //! Compute state vector in spherical coordinates.
    Eigen::Vector6d computeStateVectorInSphericalCoordinates( const double currentAzimuthAngle );

    //! Compute current normalized cartesian state.
    Eigen::Vector6d computeNormalizedStateVector( const double currentAzimuthAngle );

    //! Compute current acceleration in spherical coordinates parametrized by azimuth angle theta.
    Eigen::Vector3d computeCurrentAccelerationParametrizedByAzimuthAngle( const double currentAzimuthAngle );


    //! Compute thrust acceleration vector in spherical coordinates.
    Eigen::Vector3d computeThrustAccelerationInSphericalCoordinates( const double currentAzimuthAngle );

    //! Compute direction thrust acceleration in cartesian coordinates.
    Eigen::Vector3d computeCurrentThrustAccelerationDirection( double currentTime );

    //! Compute current thrust acceleration in normalized cartesian coordinates.
    Eigen::Vector3d computeNormalizedThrustAccelerationVector( const double currentAzimuthAngle );

    //! Time of flight function for the root-finder.
    struct TimeOfFlightFunction : public basic_mathematics::BasicFunction< double, double >
    {

        //! Create a function that returns the time of flight associated with the current trajectory.
        TimeOfFlightFunction( std::function< void ( const double ) > resetFreeCoefficientFunction,
                              std::function< double ( ) >  computeTimeOfFlightFunction,
                              std::function< double( ) > getRequiredTimeOfFlightfunction ):
        resetFreeCoefficientFunction_( resetFreeCoefficientFunction ),
        computeTimeOfFlightFunction_( computeTimeOfFlightFunction ),
        getRequiredTimeOfFlightfunction_( getRequiredTimeOfFlightfunction ){ }

        //! Evaluate the difference between the current and required time of flight values, from the current value of the free coefficient.
        double evaluate( const double inputValue )
        {
            resetFreeCoefficientFunction_( inputValue );
            double currentTimeOfFlight = computeTimeOfFlightFunction_( );

            return getRequiredTimeOfFlightfunction_() - currentTimeOfFlight; //getRequiredTimeOfFlightfunction_( ) - currentTimeOfFlight;
        }

        //! Derivative function (not provided).
        double computeDerivative( const unsigned int order, const double inputValue )
        {
            throw std::runtime_error( "The rootfinder for TOF should not evaluate derivatives!" );
        }

        //! Integral function (not provided).
        double computeDefiniteIntegral( unsigned int order, double lowerBound, double upperbound )
        {
            throw std::runtime_error( "The rootfinder for TOF should not evaluate integrals!" );
        }

        //! Get the expected true location of the root (not implemented).
        double getTrueRootLocation( ) { return TUDAT_NAN; }

        //! Get the accuracy of the true location of the root (not implemented).
        double getTrueRootAccuracy( ) { return TUDAT_NAN; }

        //! Get a reasonable initial guess of the root location (not implemented).
        double getInitialGuess( ) { return TUDAT_NAN; }

        //! Get a reasonable lower boundary for the root location (not implemented).
        double getLowerBound( ) { return TUDAT_NAN; }

        //! Get a reasonable upper boundary for the root location (not implemented).
        double getUpperBound( ) { return TUDAT_NAN; }

    protected:

    private:
        std::function< void ( const double ) > resetFreeCoefficientFunction_;
        std::function< void ( ) > satisfyBoundaryConditionsFunction_;
        std::function< double ( ) >  computeTimeOfFlightFunction_;
        std::function< double ( ) > getRequiredTimeOfFlightfunction_;
    };




private:
    //! A Initial radius in the cylindrical coordinate system
    double initialCylindricalRadius_;

    //! A Final radius in the cylindrical coordinate system
    double finalCylindricalRadius_;

    //! Unit vector of the initial position
    Eigen::Vector3d initialPositionUnit_;

    //! Unit vector of the final position
    Eigen::Vector3d finalPositionUnit_;

    //! Axis of rotation, cross product of initial and final position
    Eigen::Vector3d axisOfRotation_;

    //! A Travelled azimuthal angular distance by the trajectory
    double travelledAzimuthAngle_;

    //! The scaling factor k_0
    double scalingFactor_;

    //! The dynamic range parameter k_1
    double dynamicRangeParameter_;

    //! The winding parameter k_2
    double windingParameter_;

    //! The phase angle varphi
    double phaseAngle_;

    //! A Lower bound of gamma
    double lowerBoundGamma_;

    //! A Upper bound of gamma
    double upperBoundGamma_;

    //! A Required gamma for meeting TOF req.
    double requiredGamma_;

    //! Initial state in cartesian coordinates.
    Eigen::Vector6d initialState_;

    //! Final state in cartesian coordinates.
    Eigen::Vector6d finalState_;

    //! Targeted value for the time of flight.
    double requiredTimeOfFlight_;

    //! Number of revolutions.
    int numberOfRevolutions_;

    //! Variabale whether trajectory is feasible for given TOF
    bool infeasibleTOF_;

    //! Body map object.
    simulation_setup::NamedBodyMap bodyMap_;

    //! Name of the body to be propagated.
    std::string bodyToPropagate_;

    //! Name of the central body.
    std::string centralBody_;

    //! Central body gravitational parameter.
    double centralBodyGravitationalParameter_;

    //! Initial state in spherical coordinates.
    Eigen::Vector6d initialStateSphericalCoordinates_;

    //! Final state in spherical coordinates.
    Eigen::Vector6d finalStateSphericalCoordinates_;

    //! Initial value azimuth angle.
    double initialAzimuthAngle_;

    //! Final value azimuth angle.
    double finalAzimuthAngle_;

    //! Initial state parametrised by the azimuth angle theta.
    Eigen::Vector6d initialStateParametrizedByAzimuthAngle_;

    //! Final state parametrised by the azimuth angle theta.
    Eigen::Vector6d finalStateParametrizedByAzimuthAngle_;

    //! Pointer to the exposins shaping radial distance composite function.
    std::shared_ptr< CompositeRadialFunctionExposinsShaping > radialDistanceCompositeFunction_;

    //! Coefficients for the radial distance composite function.
    Eigen::Vector4d coefficientsRadialDistanceFunction_;

    //! Root finder settings, to be used to find the free coefficient value that ensures the time of flight is correct.
    std::shared_ptr< root_finders::RootFinderSettings > rootFinderSettings_;

    //! Integrator settings.
    std::shared_ptr< numerical_integrators::IntegratorSettings< double > > integratorSettings_;

//    //! Numerical quadrature settings, required to compute the time of flight and total deltaV.
//    std::shared_ptr< numerical_quadrature::QuadratureSettings< double > > quadratureSettings_;

    //! Inverse of matrix containing the boundary conditions
    Eigen::MatrixXd inverseMatrixBoundaryConditions_;



    std::shared_ptr< interpolators::OneDimensionalInterpolator< double, double > > interpolator_;
};


} // namespace shape_based_methods
} // namespace tudat

#endif // EXPOSINSSHAPING_H