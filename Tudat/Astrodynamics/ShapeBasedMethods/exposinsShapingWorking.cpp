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


#include "Tudat/Astrodynamics/BasicAstrodynamics/celestialBodyConstants.h"
#include "Tudat/Basics/timeType.h"
#include "Tudat/Astrodynamics/Propulsion/thrustAccelerationModel.h"
#include "exposinsShaping.h"

namespace tudat
{
namespace shape_based_methods
{


//! Constructur for exposins shaping.
ExposinsShaping::ExposinsShaping( Eigen::Vector6d initialState,
                                    Eigen::Vector6d finalState,
                                    double requiredTimeOfFlight,
                                    int numberOfRevolutions,
//                                    double centralBodyGravitationalParameter,
                                    simulation_setup::NamedBodyMap& bodyMap,
                                    const std::string bodyToPropagate,
                                    const std::string centralBody,
                                    double initialValueFreeCoefficient,
                                    std::shared_ptr< root_finders::RootFinderSettings >& rootFinderSettings,
                                    const double lowerBoundFreeCoefficient,
                                    const double upperBoundFreeCoefficient,
                                    std::shared_ptr< numerical_integrators::IntegratorSettings< double > > integratorSettings ):
    ShapeBasedMethodLeg( initialState, finalState, requiredTimeOfFlight, bodyMap, bodyToPropagate, centralBody, integratorSettings ),
    numberOfRevolutions_( numberOfRevolutions ),
    bodyMap_( bodyMap ), bodyToPropagate_( bodyToPropagate ), centralBody_( centralBody ),
    initialValueFreeCoefficient_( initialValueFreeCoefficient ),
    rootFinderSettings_( rootFinderSettings ),
    lowerBoundFreeCoefficient_( lowerBoundFreeCoefficient ),
    upperBoundFreeCoefficient_( upperBoundFreeCoefficient ),
    integratorSettings_( integratorSettings )
{
    // Retrieve gravitational parameter of the central body.
    double centralBodyGravitationalParameter = bodyMap_[ centralBody_ ]->getGravityFieldModel()->getGravitationalParameter( );

    // Normalize the initial state.
    initialState_.segment( 0, 3 ) = initialState.segment( 0, 3 ) / physical_constants::ASTRONOMICAL_UNIT;
    initialState_.segment( 3, 3 ) = initialState.segment( 3, 3 ) * physical_constants::JULIAN_YEAR / physical_constants::ASTRONOMICAL_UNIT;

    // Normalize the final state.
    finalState_.segment( 0, 3 ) = finalState.segment( 0, 3 ) / physical_constants::ASTRONOMICAL_UNIT;
    finalState_.segment( 3, 3 ) = finalState.segment( 3, 3 ) * physical_constants::JULIAN_YEAR / physical_constants::ASTRONOMICAL_UNIT;

    // Normalize the required time of flight.
    requiredTimeOfFlight_ = requiredTimeOfFlight / physical_constants::JULIAN_YEAR;

    // Normalize the gravitational parameter of the central body.
    centralBodyGravitationalParameter_ = centralBodyGravitationalParameter * std::pow( physical_constants::JULIAN_YEAR, 2.0 )
            / std::pow( physical_constants::ASTRONOMICAL_UNIT, 3.0 );


    // Compute initial state in spherical coordinates.
    initialStateSphericalCoordinates_ = coordinate_conversions::convertCartesianToSphericalState( initialState_ );

    // Compute final state in spherical coordinates.
    finalStateSphericalCoordinates_ = coordinate_conversions::convertCartesianToSphericalState( finalState_ );

    if ( initialStateSphericalCoordinates_( 1 ) < 0.0 )
    {
        initialStateSphericalCoordinates_( 1 ) += 2.0 * mathematical_constants::PI;
    }
    if ( finalStateSphericalCoordinates_( 1 ) < 0.0 )
    {
        finalStateSphericalCoordinates_( 1 ) += 2.0 * mathematical_constants::PI;
    }

    // Retrieve the initial value of the azimuth angle.
    initialAzimuthAngle_ = initialStateSphericalCoordinates_[ 1 ];

    // Compute final value of the azimuth angle.
    if ( ( finalStateSphericalCoordinates_( 1 ) - initialStateSphericalCoordinates_( 1 ) ) < 0.0 )
    {
        finalAzimuthAngle_ = finalStateSphericalCoordinates_( 1 ) + 2.0 * mathematical_constants::PI * ( numberOfRevolutions_ + 1.0 );
    }
    else
    {
        finalAzimuthAngle_ = finalStateSphericalCoordinates_( 1 ) + 2.0 * mathematical_constants::PI * ( numberOfRevolutions_ );
    }


    // Compute initial and final values of the derivative of the azimuth angle w.r.t. time.
    double initialDerivativeAzimuthAngle = initialStateSphericalCoordinates_[ 4 ]
            / ( initialStateSphericalCoordinates_[ 0 ] * std::cos( initialStateSphericalCoordinates_[ 2 ] ) );
    double finalDerivativeAzimuthAngle = finalStateSphericalCoordinates_[ 4 ]
            / ( finalStateSphericalCoordinates_[ 0 ] * std::cos( finalStateSphericalCoordinates_[ 2 ] ) );

    // Compute initial state parametrized by azimuth angle theta.
    initialStateParametrizedByAzimuthAngle_ = ( Eigen::Vector6d() << initialStateSphericalCoordinates_[ 0 ],
            initialStateSphericalCoordinates_[ 1 ],
            initialStateSphericalCoordinates_[ 2 ],
            initialStateSphericalCoordinates_[ 3 ] / initialDerivativeAzimuthAngle,
            initialStateSphericalCoordinates_[ 4 ] / initialDerivativeAzimuthAngle,
            initialStateSphericalCoordinates_[ 5 ] / initialDerivativeAzimuthAngle ).finished();

    // Compute final state parametrized by azimuth angle theta.
    finalStateParametrizedByAzimuthAngle_ = ( Eigen::Vector6d() << finalStateSphericalCoordinates_[ 0 ],
            finalStateSphericalCoordinates_[ 1 ],
            finalStateSphericalCoordinates_[ 2 ],
            finalStateSphericalCoordinates_[ 3 ] / finalDerivativeAzimuthAngle,
            finalStateSphericalCoordinates_[ 4 ] / finalDerivativeAzimuthAngle,
            finalStateSphericalCoordinates_[ 5 ] / finalDerivativeAzimuthAngle ).finished();


    // Initialise coefficients for radial distance and elevation angle functions.
    coefficientsRadialDistanceFunction_.resize( 7 );
    for ( int i = 0 ; i < 7 ; i++ )
    {
        coefficientsRadialDistanceFunction_[ i ] = 1.0;
    }
    coefficientsElevationAngleFunction_.resize( 4 );
    for ( int i = 0 ; i < 4 ; i++ )
    {
        coefficientsElevationAngleFunction_[ i ] = 1.0;
    }


    // Define coefficients for radial distance and elevation angle composite functions.
    radialDistanceCompositeFunction_ = std::make_shared< CompositeRadialFunctionExposinsShaping >( coefficientsRadialDistanceFunction_ );
    elevationAngleCompositeFunction_ = std::make_shared< CompositeElevationFunctionExposinsShaping >( coefficientsElevationAngleFunction_ );


    // Define settings for numerical quadrature, to be used to compute time of flight and final deltaV.
    quadratureSettings_ = std::make_shared< numerical_quadrature::GaussianQuadratureSettings < double > >( initialAzimuthAngle_, 16 );

    // Iterate on the free coefficient value until the time of flight matches its required value.
    iterateToMatchRequiredTimeOfFlight( rootFinderSettings_, lowerBoundFreeCoefficient_, upperBoundFreeCoefficient_, initialValueFreeCoefficient_ );




    // /////////////////////////////////////////////////
    //A Travelled azimuth angle

    travelledAzimuthAngle_ = mathematical_constants::PI/2 + 2*mathematical_constants::PI*2;  //finalAzimuthAngle_ - initialAzimuthAngle_;

    //A Compute lower/upper bounds on gamma
    windingParameter_ = 0.08333333333;
    initialCylindricalRadius_ = 1.;
    finalCylindricalRadius_   = 1.5;

    lowerBoundGamma_ = std::atan(windingParameter_/2*(-log(initialCylindricalRadius_/finalCylindricalRadius_)*(1/std::tan((windingParameter_*travelledAzimuthAngle_)/2)) - std::sqrt((2*(1-std::cos(windingParameter_*travelledAzimuthAngle_)))/std::pow(windingParameter_,4) - std::pow(std::log(initialCylindricalRadius_/finalCylindricalRadius_),2))));
    upperBoundGamma_ = std::atan(windingParameter_/2*(-log(initialCylindricalRadius_/finalCylindricalRadius_)*(1/std::tan((windingParameter_*travelledAzimuthAngle_)/2)) + std::sqrt((2*(1-std::cos(windingParameter_*travelledAzimuthAngle_)))/std::pow(windingParameter_,4) - std::pow(std::log(initialCylindricalRadius_/finalCylindricalRadius_),2))));


    requiredGamma_ = (lowerBoundGamma_ + upperBoundGamma_)/2;
    requiredGamma_ = -80*mathematical_constants::PI/180;
    requiredGamma_ = 0.;

    quadratureSettings_ = std::make_shared< numerical_quadrature::GaussianQuadratureSettings < double > >( 0, 16 );
    iterateToMatchRequiredTimeOfFlightExposins( rootFinderSettings_, lowerBoundGamma_, upperBoundGamma_, requiredGamma_ );

    requiredGamma_ = -25.0*mathematical_constants::PI/180;

//    double requiredGamma = -80*mathem*atical_constants::PI/180;

//    double windTravelProduct = windingParameter_*travelledAzimuthAngle_;
//    double logRadiiFraction = std::log(initialCylindricalRadius_/finalCylindricalRadius_);

//    double dynamicRangeValue = std::sqrt(std::pow((logRadiiFraction + (std::tan(requiredGamma)/windingParameter_)*std::sin(windTravelProduct))/(1-std::cos(windTravelProduct)),2) + std::pow(std::tan(requiredGamma),2)/std::pow(windingParameter_,2));
//    double dynamicRangeSign  = (logRadiiFraction + (std::tan(requiredGamma)/windingParameter_)*std::sin(windTravelProduct));
//    double dynamicRangeParameter = std::copysign(dynamicRangeValue,dynamicRangeSign); // possible problems with zero

//    double phaseAngle = std::acos(std::tan(requiredGamma)/(windingParameter_*dynamicRangeParameter));

//    double scalingFactor = physical_cons*/tants::ASTRONOMICAL_UNIT*initialCylindricalRadius_/(std::exp(dynamicRangeParameter*std::sin(phaseAngle))); // Unit is meter [m]

//    computeBoundsGamma( initialCylindricalRadius_, finalCylindricalRadius_, windingParameter_,travelledAzimuthAngle_)



    std::cout << std::endl << initialAzimuthAngle_*180/mathematical_constants::PI << std::endl;
    std::cout << "finalAzimuthAngle_ " << finalAzimuthAngle_*180/mathematical_constants::PI << std::endl;
    std::cout << "numberOfRevolutions_ " << numberOfRevolutions_ << std::endl;
    std::cout << "travelledAzimuthAngle_ " << travelledAzimuthAngle_*180/mathematical_constants::PI << std::endl;
    std::cout << "windingParameter_ " << windingParameter_ << std::endl;
    std::cout << "initialCylindricalRadius_ " << initialCylindricalRadius_ << std::endl;
    std::cout << "finalCylindricalRadius_ " << finalCylindricalRadius_ << std::endl;
    std::cout << "lowerBoundGamma_ " << lowerBoundGamma_ << std::endl;
    std::cout << "upperBoundGamma_ " << upperBoundGamma_ << std::endl;

//    std::cout << "requiredGamma " << requiredGamma << std::endl;
//    std::cout << "dynamicRangeValue " << dynamicRangeValue << std::endl;
//    std::cout << "dynamicRangeSign " << dynamicRangeSign << std::endl;
//    std::cout << "dynamicRangeParameter " << dynamicRangeParameter << std::endl;
//    std::cout << "phaseAngle " << phaseAngle << std::endl;
//    std::cout << "scalingFactor " << scalingFactor << std::endl;



    std::cout << "requiredGamma_ " << requiredGamma_ << std::endl;
//    std::cout << computeNormalizedTimeOfFlightExposins() << std::endl;
//    requiredGamma_ = lowerBoundGamma_;
//    std::cout << computeNormalizedTimeOfFlightExposins() << std::endl;
//    requiredGamma_ = upperBoundGamma_;
//    std::cout << computeNormalizedTimeOfFlightExposins() << std::endl;


    // compare with slides 69 and 71 exposinsv4-20
//    std::cout << "computeDeltaVExposins " << computeDeltaVExposins() << std::endl;
    std::cout << "computeCurrentNormalizedThrustAccelerationMagnitude 0 " << computeCurrentNormalizedThrustAccelerationMagnitude(0) << " [m/s^2]" << std::endl;
    std::cout << "computeFirstDerivativeAzimuthAngleWrtTimeExposins 0 " << 1.0/std::pow(computeFirstDerivativeAzimuthAngleWrtTimeExposins(0),2.0) << " [s^2/rad^2]"  << std::endl;

    std::cout << "computeCurrentNormalizedThrustAccelerationMagnitude 0.7068583 " << computeCurrentNormalizedThrustAccelerationMagnitude( 0.7068583 ) << " [m/s^2]" << std::endl;
    std::cout << "computeFirstDerivativeAzimuthAngleWrtTimeExposins 0.7068583 " << 1.0/std::pow(computeFirstDerivativeAzimuthAngleWrtTimeExposins( 0.7068583 ),2.0) << " [s^2/rad^2]"  << std::endl;

    std::cout << "computeCurrentNormalizedThrustAccelerationMagnitude 1.4137167 " << computeCurrentNormalizedThrustAccelerationMagnitude( 1.4137167 ) << " [m/s^2]" << std::endl;
    std::cout << "computeFirstDerivativeAzimuthAngleWrtTimeExposins 1.4137167 " << 1.0/std::pow(computeFirstDerivativeAzimuthAngleWrtTimeExposins( 1.4137167 ),2.0) << " [s^2/rad^2]"   << std::endl;

    std::cout << "computeDeltaVExposins " << computeDeltaVExposins() << " [m/s]"<< std::endl;

    quadratureSettings_ = std::make_shared< numerical_quadrature::GaussianQuadratureSettings < double > >( initialAzimuthAngle_, 16 );

    // ///////////////////////////////////////




    // Retrieve initial step size.
    double initialStepSize = integratorSettings->initialTimeStep_;

    // Vector of azimuth angles at which the time should be computed.
    Eigen::VectorXd azimuthAnglesToComputeAssociatedEpochs =
            Eigen::VectorXd::LinSpaced( std::ceil( computeNormalizedTimeOfFlight() * physical_constants::JULIAN_YEAR / initialStepSize ),
                                        initialAzimuthAngle_, finalAzimuthAngle_ );

    std::map< double, double > dataToInterpolate;
    for ( int i = 0 ; i < azimuthAnglesToComputeAssociatedEpochs.size() ; i++ )
    {
        dataToInterpolate[ computeCurrentTimeFromAzimuthAngle( azimuthAnglesToComputeAssociatedEpochs[ i ] ) * physical_constants::JULIAN_YEAR ]
                = azimuthAnglesToComputeAssociatedEpochs[ i ];
    }

    // Create interpolator.
    std::shared_ptr< interpolators::InterpolatorSettings > interpolatorSettings =
            std::make_shared< interpolators::LagrangeInterpolatorSettings >( 10 );

    interpolator_ = interpolators::createOneDimensionalInterpolator( dataToInterpolate, interpolatorSettings );



}


//! Convert time to independent variable.
double ExposinsShaping::convertTimeToIndependentVariable( const double time )
{
//    // Retrieve initial step size.
//    double initialStepSize = integratorSettings_->initialTimeStep_;

//    // Vector of azimuth angles at which the time should be computed.
//    Eigen::VectorXd azimuthAnglesToComputeAssociatedEpochs =
//            Eigen::VectorXd::LinSpaced( std::ceil( computeNormalizedTimeOfFlight( ) * physical_constants::JULIAN_YEAR / initialStepSize ),
//                                        initialAzimuthAngle_, finalAzimuthAngle_ );

//    std::map< double, double > dataToInterpolate;
//    for ( int i = 0 ; i < azimuthAnglesToComputeAssociatedEpochs.size() ; i++ )
//    {
//        dataToInterpolate[ computeCurrentTimeFromAzimuthAngle( azimuthAnglesToComputeAssociatedEpochs[ i ] ) * physical_constants::JULIAN_YEAR ]
//                = azimuthAnglesToComputeAssociatedEpochs[ i ];
//    }

//    // Create interpolator.
//    std::shared_ptr< interpolators::InterpolatorSettings > interpolatorSettings =
//            std::make_shared< interpolators::LagrangeInterpolatorSettings >( 10 );

//    std::shared_ptr< interpolators::OneDimensionalInterpolator< double, double > > interpolator =
//            interpolators::createOneDimensionalInterpolator( dataToInterpolate, interpolatorSettings );

//    double independentVariable = interpolator->interpolate( time );

    return  interpolator_->interpolate( time ); //independentVariable;
}


//! Convert independent variable to time.
double ExposinsShaping::convertIndependentVariableToTime( const double independentVariable )
{
    // Define the derivative of the time function w.r.t the azimuth angle theta.
    std::function< double( const double ) > derivativeTimeFunction = [ = ] ( const double currentAzimuthAngle ){

        double scalarFunctionTimeEquation = computeScalarFunctionTimeEquation( currentAzimuthAngle );

        // Check that the trajectory is feasible, ie curved toward the central body.
        if ( scalarFunctionTimeEquation < 0.0 )
        {
            throw std::runtime_error ( "Error, trajectory not curved toward the central body, and thus not feasible." );
        }
        else
        {
            return std::sqrt( computeScalarFunctionTimeEquation( currentAzimuthAngle ) *
                              std::pow( radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle ), 2.0 )
                              / centralBodyGravitationalParameter_ );
        }
    };

    // Create numerical quadrature from quadrature settings.
    std::shared_ptr< numerical_quadrature::NumericalQuadrature< double, double > > quadrature =
            numerical_quadrature::createQuadrature( derivativeTimeFunction, quadratureSettings_, independentVariable );

    double time = quadrature->getQuadrature( );

    return time;
}


Eigen::MatrixXd ExposinsShaping::computeInverseMatrixBoundaryConditions( )
{
    Eigen::MatrixXd matrixBoundaryConditions = Eigen::MatrixXd::Zero( 10, 10 );

    for ( int i = 0 ; i < 6 ; i++ )
    {
        int index = i;
        if ( i >= 2 )
        {
            index = i + 1;
        }
        matrixBoundaryConditions( 0, i ) = radialDistanceCompositeFunction_->getComponentFunctionCurrentValue( index, initialAzimuthAngle_ );
        matrixBoundaryConditions( 1, i ) = radialDistanceCompositeFunction_->getComponentFunctionCurrentValue( index, finalAzimuthAngle_ );
        matrixBoundaryConditions( 2, i ) = radialDistanceCompositeFunction_->getComponentFunctionFirstDerivative( index, initialAzimuthAngle_ );
        matrixBoundaryConditions( 3, i ) = radialDistanceCompositeFunction_->getComponentFunctionFirstDerivative( index, finalAzimuthAngle_ );
        matrixBoundaryConditions( 4, i ) = - std::pow( initialStateSphericalCoordinates_[ 0 ], 2.0 )
                * radialDistanceCompositeFunction_->getComponentFunctionSecondDerivative( index, initialAzimuthAngle_ );
        matrixBoundaryConditions( 5, i ) = - std::pow( finalStateSphericalCoordinates_[ 0 ], 2.0 )
                * radialDistanceCompositeFunction_->getComponentFunctionSecondDerivative( index, finalAzimuthAngle_ );
    }

    // Compute value of variable alpha at initial time.
    double initialValueAlpha = computeInitialAlphaValue();

    // Compute value of variable alpha at final time.
    double finalValueAlpha = computeFinalAlphaValue();

    for ( int i = 0 ; i < 4 ; i++ )
    {
        matrixBoundaryConditions( 4, i + 6 ) =
                initialValueAlpha * elevationAngleCompositeFunction_->getComponentFunctionSecondDerivative( i, initialAzimuthAngle_ );
        matrixBoundaryConditions( 5, i + 6 ) =
                finalValueAlpha * elevationAngleCompositeFunction_->getComponentFunctionSecondDerivative( i, finalAzimuthAngle_ );
        matrixBoundaryConditions( 6, i + 6 ) = elevationAngleCompositeFunction_->getComponentFunctionCurrentValue( i, initialAzimuthAngle_ );
        matrixBoundaryConditions( 7, i + 6 ) = elevationAngleCompositeFunction_->getComponentFunctionCurrentValue( i, finalAzimuthAngle_ );
        matrixBoundaryConditions( 8, i + 6 ) = elevationAngleCompositeFunction_->getComponentFunctionFirstDerivative( i, initialAzimuthAngle_ );
        matrixBoundaryConditions( 9, i + 6 ) = elevationAngleCompositeFunction_->getComponentFunctionFirstDerivative( i, finalAzimuthAngle_ );
    }

    // Compute and return the inverse of the boundary conditions matrix.
    return matrixBoundaryConditions.inverse();
}

double ExposinsShaping::computeInitialAlphaValue( )
{
    return - ( initialStateParametrizedByAzimuthAngle_[ 3 ] * initialStateParametrizedByAzimuthAngle_[ 5 ] / initialStateParametrizedByAzimuthAngle_[ 0 ] )
            / ( std::pow( initialStateParametrizedByAzimuthAngle_[ 5 ] / initialStateParametrizedByAzimuthAngle_[ 0 ], 2.0 )
            + std::pow( std::cos( initialStateParametrizedByAzimuthAngle_[ 2 ] ), 2.0 ) );
}

double ExposinsShaping::computeFinalAlphaValue( )
{
    return - ( finalStateParametrizedByAzimuthAngle_[ 3 ] *  finalStateParametrizedByAzimuthAngle_[ 5 ] / finalStateParametrizedByAzimuthAngle_[ 0 ] )
            / ( std::pow( finalStateParametrizedByAzimuthAngle_[ 5 ] / finalStateParametrizedByAzimuthAngle_[ 0 ], 2.0 )
            + std::pow( std::cos( finalStateParametrizedByAzimuthAngle_[ 2 ] ), 2.0 ) );
}

double ExposinsShaping::computeInitialValueBoundariesConstant( )
{
    double radialDistance = initialStateParametrizedByAzimuthAngle_[ 0 ];
    double elevationAngle = initialStateParametrizedByAzimuthAngle_[ 2 ];
    double derivativeRadialDistance = initialStateParametrizedByAzimuthAngle_[ 3 ];
    double derivativeElevationAngle = initialStateParametrizedByAzimuthAngle_[ 5 ] / initialStateParametrizedByAzimuthAngle_[ 0 ];
    double derivativeOfTimeWrtAzimuthAngle = ( initialStateParametrizedByAzimuthAngle_[ 0 ] * std::cos( initialStateParametrizedByAzimuthAngle_[ 2 ] ) )
            / initialStateSphericalCoordinates_[ 4 ];

    return - centralBodyGravitationalParameter_ * std::pow( derivativeOfTimeWrtAzimuthAngle, 2.0 ) / std::pow( radialDistance, 2.0 )
            + 2.0 * std::pow( derivativeRadialDistance, 2.0 ) / radialDistance
            + radialDistance * ( std::pow( derivativeElevationAngle, 2.0 ) + std::pow( std::cos( elevationAngle ), 2.0 ) )
            - derivativeRadialDistance * derivativeElevationAngle * ( std::sin( elevationAngle ) * std::cos( elevationAngle ) )
            / ( std::pow( derivativeElevationAngle, 2.0 ) + std::pow( std::cos( elevationAngle ), 2.0 ) );
}

double ExposinsShaping::computeFinalValueBoundariesConstant( )
{
    double radialDistance = finalStateParametrizedByAzimuthAngle_[ 0 ];
    double elevationAngle = finalStateParametrizedByAzimuthAngle_[ 2 ];
    double derivativeRadialDistance = finalStateParametrizedByAzimuthAngle_[ 3 ];
    double derivativeElevationAngle = finalStateParametrizedByAzimuthAngle_[ 5 ] / finalStateParametrizedByAzimuthAngle_[ 0 ];
    double derivativeOfTimeWrtAzimuthAngle = ( finalStateParametrizedByAzimuthAngle_[ 0 ] * std::cos( finalStateParametrizedByAzimuthAngle_[ 2 ] ) )
            / finalStateSphericalCoordinates_[ 4 ];

    return - centralBodyGravitationalParameter_ * std::pow( derivativeOfTimeWrtAzimuthAngle, 2.0 ) / std::pow( radialDistance, 2.0 )
            + 2.0 * std::pow( derivativeRadialDistance, 2.0 ) / radialDistance
            + radialDistance * ( std::pow( derivativeElevationAngle, 2.0 ) + std::pow( std::cos( elevationAngle ), 2.0 ) )
            - derivativeRadialDistance * derivativeElevationAngle * ( std::sin( elevationAngle ) * std::cos( elevationAngle ) )
            / ( std::pow( derivativeElevationAngle, 2.0 ) + std::pow( std::cos( elevationAngle ), 2.0 ) );
}

void ExposinsShaping::satisfyBoundaryConditions( )
{
    Eigen::VectorXd vectorBoundaryValues;
    vectorBoundaryValues.resize( 10 );

    vectorBoundaryValues[ 0 ] = 1.0 / initialStateParametrizedByAzimuthAngle_[ 0 ];
    vectorBoundaryValues[ 1 ] = 1.0 / finalStateParametrizedByAzimuthAngle_[ 0 ];
    vectorBoundaryValues[ 2 ] = - initialStateParametrizedByAzimuthAngle_[ 3 ] / std::pow( initialStateParametrizedByAzimuthAngle_[ 0 ], 2.0 );
    vectorBoundaryValues[ 3 ] = - finalStateParametrizedByAzimuthAngle_[ 3 ] / std::pow( finalStateParametrizedByAzimuthAngle_[ 0 ], 2.0 );
    vectorBoundaryValues[ 4 ] = computeInitialValueBoundariesConstant()
            - 2.0 * std::pow( initialStateParametrizedByAzimuthAngle_[ 3 ], 2.0 ) / initialStateParametrizedByAzimuthAngle_[ 0 ];
    vectorBoundaryValues[ 5 ] = computeFinalValueBoundariesConstant()
            - 2.0 * std::pow( finalStateParametrizedByAzimuthAngle_[ 3 ], 2.0 ) / finalStateParametrizedByAzimuthAngle_[ 0 ];
    vectorBoundaryValues[ 6 ] = initialStateParametrizedByAzimuthAngle_[ 2 ];
    vectorBoundaryValues[ 7 ] = finalStateParametrizedByAzimuthAngle_[ 2 ];
    vectorBoundaryValues[ 8 ] = initialStateParametrizedByAzimuthAngle_[ 5 ] / initialStateParametrizedByAzimuthAngle_[ 0 ];
    vectorBoundaryValues[ 9 ] = finalStateParametrizedByAzimuthAngle_[ 5 ] / finalStateParametrizedByAzimuthAngle_[ 0 ];

    Eigen::VectorXd vectorSecondComponentContribution;
    vectorSecondComponentContribution.resize( 10.0 );

    vectorSecondComponentContribution[ 0 ] = radialDistanceCompositeFunction_->getComponentFunctionCurrentValue( 2, initialAzimuthAngle_ );
    vectorSecondComponentContribution[ 1 ] = radialDistanceCompositeFunction_->getComponentFunctionCurrentValue( 2, finalAzimuthAngle_ );
    vectorSecondComponentContribution[ 2 ] = radialDistanceCompositeFunction_->getComponentFunctionFirstDerivative( 2, initialAzimuthAngle_ );
    vectorSecondComponentContribution[ 3 ] = radialDistanceCompositeFunction_->getComponentFunctionFirstDerivative( 2, finalAzimuthAngle_ );
    vectorSecondComponentContribution[ 4 ] = - std::pow( initialStateParametrizedByAzimuthAngle_[ 0 ], 2 )
              * radialDistanceCompositeFunction_->getComponentFunctionSecondDerivative( 2, initialAzimuthAngle_ );
    vectorSecondComponentContribution[ 5 ] = - std::pow( finalStateParametrizedByAzimuthAngle_[ 0 ], 2 )
              * radialDistanceCompositeFunction_->getComponentFunctionSecondDerivative( 2, finalAzimuthAngle_ );
    vectorSecondComponentContribution[ 6 ] = 0.0;
    vectorSecondComponentContribution[ 7 ] = 0.0;
    vectorSecondComponentContribution[ 8 ] = 0.0;
    vectorSecondComponentContribution[ 9 ] = 0.0;

    vectorSecondComponentContribution *= initialValueFreeCoefficient_;

    Eigen::MatrixXd inverseMatrixBoundaryConditions_ = computeInverseMatrixBoundaryConditions();

    Eigen::MatrixXd compositeFunctionCoefficients = inverseMatrixBoundaryConditions_ * ( vectorBoundaryValues - vectorSecondComponentContribution );

    for ( int i = 0 ; i < 6 ; i++ )
    {
        if ( i < 2 )
        {
            coefficientsRadialDistanceFunction_( i ) = compositeFunctionCoefficients( i );
        }
        else
        {
            coefficientsRadialDistanceFunction_( i + 1 ) = compositeFunctionCoefficients( i );
        }
    }
    coefficientsRadialDistanceFunction_( 2 ) = initialValueFreeCoefficient_;

    for ( int i = 0 ; i < 4 ; i++ )
    {
        coefficientsElevationAngleFunction_( i ) = compositeFunctionCoefficients( i + 6 );
    }

    radialDistanceCompositeFunction_->resetCompositeFunctionCoefficients( coefficientsRadialDistanceFunction_ );
    elevationAngleCompositeFunction_->resetCompositeFunctionCoefficients( coefficientsElevationAngleFunction_ );

}



double ExposinsShaping::computeScalarFunctionTimeEquation( double currentAzimuthAngle )
{
    double radialFunctionValue = radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle );
    double firstDerivativeRadialFunction = radialDistanceCompositeFunction_->evaluateCompositeFunctionFirstDerivative( currentAzimuthAngle );
    double secondDerivativeRadialFunction = radialDistanceCompositeFunction_->evaluateCompositeFunctionSecondDerivative( currentAzimuthAngle );

    double elevationFunctionValue = elevationAngleCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle );
    double firstDerivativeElevationFunction = elevationAngleCompositeFunction_->evaluateCompositeFunctionFirstDerivative( currentAzimuthAngle );
    double secondDerivativeElevationFunction = elevationAngleCompositeFunction_->evaluateCompositeFunctionSecondDerivative( currentAzimuthAngle );

    return - secondDerivativeRadialFunction + 2.0 * std::pow( firstDerivativeRadialFunction, 2.0 ) / radialFunctionValue
            + firstDerivativeRadialFunction * firstDerivativeElevationFunction
            * ( secondDerivativeElevationFunction - std::sin( elevationFunctionValue ) * std::cos( elevationFunctionValue ) )
            / ( std::pow( firstDerivativeElevationFunction, 2.0 ) + std::pow( std::cos( elevationFunctionValue ), 2.0 ) )
            + radialFunctionValue * ( std::pow( firstDerivativeElevationFunction, 2.0 ) + std::pow( std::cos( elevationFunctionValue ), 2.0 ) );
}

double ExposinsShaping::computeDerivativeScalarFunctionTimeEquation( double currentAzimuthAngle )
{
    double radialFunctionValue = radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle );
    double firstDerivativeRadialFunction = radialDistanceCompositeFunction_->evaluateCompositeFunctionFirstDerivative( currentAzimuthAngle );
    double secondDerivativeRadialFunction = radialDistanceCompositeFunction_->evaluateCompositeFunctionSecondDerivative( currentAzimuthAngle );
    double thirdDerivativeRadialFunction = radialDistanceCompositeFunction_->evaluateCompositeFunctionThirdDerivative( currentAzimuthAngle );

    double elevationFunctionValue = elevationAngleCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle );
    double firstDerivativeElevationFunction = elevationAngleCompositeFunction_->evaluateCompositeFunctionFirstDerivative( currentAzimuthAngle );
    double secondDerivativeElevationFunction = elevationAngleCompositeFunction_->evaluateCompositeFunctionSecondDerivative( currentAzimuthAngle );
    double thirdDerivativeElevationFunction = elevationAngleCompositeFunction_->evaluateCompositeFunctionThirdDerivative( currentAzimuthAngle );

    // Define constants F1, F2, F3 and F4 as proposed in... (ADD REFERENCE).
    double F1 = std::pow( firstDerivativeElevationFunction, 2.0 ) + std::pow( std::cos( elevationFunctionValue ), 2.0 );
    double F2 = secondDerivativeElevationFunction - std::sin( 2.0 * elevationFunctionValue ) / 2.0;
    double F3 = std::cos( 2.0 * elevationFunctionValue ) + 2.0 * std::pow( firstDerivativeElevationFunction, 2.0 ) + 1.0;
    double F4 = 2.0 * secondDerivativeElevationFunction - std::sin( 2.0 * elevationFunctionValue );

    return  F1 * firstDerivativeRadialFunction - thirdDerivativeRadialFunction
            - 2.0 * std::pow( firstDerivativeRadialFunction, 3.0 ) / std::pow( radialFunctionValue, 2.0 )
            + 4.0 * firstDerivativeRadialFunction * secondDerivativeRadialFunction / radialFunctionValue
            + F4 * firstDerivativeElevationFunction * radialFunctionValue
            + 2.0 * firstDerivativeElevationFunction * firstDerivativeRadialFunction
            * ( thirdDerivativeElevationFunction - firstDerivativeElevationFunction * std::cos( 2.0 * elevationFunctionValue ) ) / F3
            + F2 * firstDerivativeElevationFunction * secondDerivativeRadialFunction / F1
            + F2 * firstDerivativeRadialFunction * secondDerivativeElevationFunction / F1
            - 4.0 * F4 * F2 * std::pow( firstDerivativeElevationFunction, 2.0 ) * firstDerivativeRadialFunction / std::pow( F3, 2.0 );
}

double ExposinsShaping::computeNormalizedTimeOfFlight()
{

    // Define the derivative of the time function w.r.t the azimuth angle theta.
    std::function< double( const double ) > derivativeTimeFunction = [ = ] ( const double currentAzimuthAngle ){

        double scalarFunctionTimeEquation = computeScalarFunctionTimeEquation( currentAzimuthAngle );

        // Check that the trajectory is feasible, ie curved toward the central body.
        if ( scalarFunctionTimeEquation < 0.0 )
        {
            throw std::runtime_error ( "Error, trajectory not curved toward the central body, and thus not feasible." );
        }
        else
        {
            return std::sqrt( computeScalarFunctionTimeEquation( currentAzimuthAngle ) *
                              std::pow( radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle ), 2.0 )
                              / centralBodyGravitationalParameter_ );
        }
    };

    // Create numerical quadrature from quadrature settings.
    std::shared_ptr< numerical_quadrature::NumericalQuadrature< double, double > > quadrature =
            numerical_quadrature::createQuadrature( derivativeTimeFunction, quadratureSettings_, finalAzimuthAngle_ );

    return quadrature->getQuadrature( );
}


double ExposinsShaping::computeBaseTimeOfFlightFunction( double currentAzimuthAngle )
{

    double windTravelProduct = windingParameter_*travelledAzimuthAngle_;
    double logRadiiFraction = std::log(initialCylindricalRadius_/finalCylindricalRadius_);

    double dynamicRangeValue = std::sqrt(std::pow((logRadiiFraction + (std::tan(requiredGamma_)/windingParameter_)*std::sin(windTravelProduct))/(1-std::cos(windTravelProduct)),2) + std::pow(std::tan(requiredGamma_),2)/std::pow(windingParameter_,2));
    double dynamicRangeSign  = (logRadiiFraction + (std::tan(requiredGamma_)/windingParameter_)*std::sin(windTravelProduct));
    double dynamicRangeParameter = std::copysign(dynamicRangeValue,dynamicRangeSign); // possible problems with zero

    double phaseAngle = std::acos(std::tan(requiredGamma_)/(windingParameter_*dynamicRangeParameter));

    double scalingFactor = initialCylindricalRadius_/(std::exp(dynamicRangeParameter*std::sin(phaseAngle))); // Unit is meter [m] if multiplied with physical_constants::ASTRONOMICAL_UNIT*


    double cosineValue = std::cos(windingParameter_*currentAzimuthAngle);
    double sineValue   = std::sin(windingParameter_*currentAzimuthAngle + phaseAngle);
    double flightAngle = std::atan(dynamicRangeParameter*windingParameter_*cosineValue);
    double radiusValue = scalingFactor*std::exp(dynamicRangeParameter*sineValue);

    return std::sqrt(std::pow(radiusValue,3)*
                     (std::pow(std::tan(flightAngle),2) + dynamicRangeParameter*std::pow(windingParameter_,2)*sineValue + 1)
                     /centralBodyGravitationalParameter_);
}

double ExposinsShaping::computeNormalizedTimeOfFlightExposins()
{

    // Define the derivative of the time function w.r.t the azimuth angle theta.
    std::function< double( const double ) > derivativeTimeFunction = [ = ] ( const double currentAzimuthAngle )
    {
        return computeBaseTimeOfFlightFunction( currentAzimuthAngle );
    };

    // Create numerical quadrature from quadrature settings.
    std::shared_ptr< numerical_quadrature::NumericalQuadrature< double, double > > quadrature =
            numerical_quadrature::createQuadrature( derivativeTimeFunction, quadratureSettings_, travelledAzimuthAngle_ );

    return quadrature->getQuadrature( );
}



//! Compute current time from azimuth angle.
double ExposinsShaping::computeCurrentTimeFromAzimuthAngle( const double currentAzimuthAngle )
{
    // Define the derivative of the time function w.r.t the azimuth angle theta.
    std::function< double( const double ) > derivativeTimeFunction = [ = ] ( const double currentAzimuthAngle ){

        double scalarFunctionTimeEquation = computeScalarFunctionTimeEquation( currentAzimuthAngle );

        // Check that the trajectory is feasible, ie curved toward the central body.
        if ( scalarFunctionTimeEquation < 0.0 )
        {
            throw std::runtime_error ( "Error, trajectory not curved toward the central body, and thus not feasible." );
        }
        else
        {
            return std::sqrt( computeScalarFunctionTimeEquation( currentAzimuthAngle ) *
                              std::pow( radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle ), 2.0 )
                              / centralBodyGravitationalParameter_ );
        }
    };

    // Create numerical quadrature from quadrature settings.
    std::shared_ptr< numerical_quadrature::NumericalQuadrature< double, double > > quadrature =
            numerical_quadrature::createQuadrature( derivativeTimeFunction, quadratureSettings_, currentAzimuthAngle );

    return quadrature->getQuadrature( );
}

//! Iterate to match the required time of flight
void ExposinsShaping::iterateToMatchRequiredTimeOfFlight( std::shared_ptr< root_finders::RootFinderSettings > rootFinderSettings,
                                                           const double lowerBound,
                                                           const double upperBound,
                                                           const double initialGuess )
{

    // Define the structure updating the time of flight from the free coefficient value, while still satisfying the boundary conditions.
    std::function< void ( const double ) > resetFreeCoefficientFunction = std::bind( &ExposinsShaping::resetValueFreeCoefficient, this, std::placeholders::_1 );
    std::function< void( ) > satisfyBoundaryConditionsFunction = std::bind( &ExposinsShaping::satisfyBoundaryConditions, this );
    std::function< double ( ) >  computeTOFfunction = std::bind( &ExposinsShaping::computeNormalizedTimeOfFlight, this );
    std::function< double ( ) > getRequiredTOFfunction = std::bind( &ExposinsShaping::getNormalizedRequiredTimeOfFlight, this );

    std::shared_ptr< basic_mathematics::Function< double, double > > timeOfFlightFunction =
            std::make_shared< ExposinsShaping::TimeOfFlightFunction >( resetFreeCoefficientFunction, satisfyBoundaryConditionsFunction, computeTOFfunction, getRequiredTOFfunction );

    // Create root finder from root finder settings.
    std::shared_ptr< root_finders::RootFinderCore< double > > rootFinder = root_finders::createRootFinder( rootFinderSettings, lowerBound, upperBound, initialGuess );

    // Iterate to find the free coefficient value that matches the required time of flight.
    double updatedFreeCoefficient = rootFinder->execute( timeOfFlightFunction, initialGuess );

}

//! Iterate to match the required time of flight
void ExposinsShaping::iterateToMatchRequiredTimeOfFlightExposins( std::shared_ptr< root_finders::RootFinderSettings > rootFinderSettings,
                                                           const double lowerBound,
                                                           const double upperBound,
                                                           const double initialGuess )
{

    // Define the structure updating the time of flight from the free coefficient value, while still satisfying the boundary conditions.
    std::function< void ( const double ) > resetRequiredGammaFunction = std::bind( &ExposinsShaping::resetRequiredGamma, this, std::placeholders::_1 );
    std::function< void( ) > satisfyBoundaryConditionsFunction = std::bind( &ExposinsShaping::satisfyBoundaryConditions, this );
    std::function< double ( ) >  computeTOFfunction = std::bind( &ExposinsShaping::computeNormalizedTimeOfFlightExposins, this );
    std::function< double ( ) > getRequiredTOFfunction = std::bind( &ExposinsShaping::getNormalizedRequiredTimeOfFlight, this );

    std::shared_ptr< basic_mathematics::Function< double, double > > timeOfFlightFunction =
            std::make_shared< ExposinsShaping::TimeOfFlightFunctionExposins >( resetRequiredGammaFunction, satisfyBoundaryConditionsFunction, computeTOFfunction, getRequiredTOFfunction );

    // Create root finder from root finder settings.
    std::shared_ptr< root_finders::RootFinderCore< double > > rootFinder = root_finders::createRootFinder( rootFinderSettings, lowerBound, upperBound, initialGuess );

    // Iterate to find the free coefficient value that matches the required time of flight.
    double updatedRequiredGamma = rootFinder->execute( timeOfFlightFunction, initialGuess );

}

//! Compute current spherical position.
Eigen::Vector3d ExposinsShaping::computePositionVectorInSphericalCoordinates( const double currentAzimuthAngle )
{

    return ( Eigen::Vector3d() <<
             radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle ),
             currentAzimuthAngle,
             elevationAngleCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle ) ).finished();

}

//! Compute current derivative of the azimuth angle.
double ExposinsShaping::computeFirstDerivativeAzimuthAngleWrtTime( const double currentAzimuthAngle )
{
    // Compute scalar function time equation.
    double scalarFunctionTimeEquation = computeScalarFunctionTimeEquation( currentAzimuthAngle );

    // Compute current radial distance.
    double radialDistance = radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle );

    // Compute and return the first derivative of the azimuth angle w.r.t. time.
    return std::sqrt( centralBodyGravitationalParameter_ / ( scalarFunctionTimeEquation * std::pow( radialDistance, 2.0 ) ) );
}

//! Compute second derivative of the azimuth angle.
double ExposinsShaping::computeSecondDerivativeAzimuthAngleWrtTime( const double currentAzimuthAngle )
{
    // Compute first derivative azimuth angle w.r.t. time.
    double firstDerivativeAzimuthAngle = computeFirstDerivativeAzimuthAngleWrtTime( currentAzimuthAngle );

    // Compute scalar function of the time equation, and its derivative w.r.t. azimuth angle.
    double scalarFunctionTimeEquation = computeScalarFunctionTimeEquation( currentAzimuthAngle );
    double derivativeScalarFunctionTimeEquation = computeDerivativeScalarFunctionTimeEquation( currentAzimuthAngle );

    // Compute radial distance, and its derivative w.r.t. azimuth angle.
    double radialDistance = radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle );
    double firstDerivativeRadialDistance = radialDistanceCompositeFunction_->evaluateCompositeFunctionFirstDerivative( currentAzimuthAngle );

    return - std::pow( firstDerivativeAzimuthAngle, 2.0 )
            * ( derivativeScalarFunctionTimeEquation / ( 2.0 * scalarFunctionTimeEquation ) + firstDerivativeRadialDistance / radialDistance );
}


//! Compute current velocity in spherical coordinates.
Eigen::Vector3d ExposinsShaping::computeVelocityVectorInSphericalCoordinates(const double currentAzimuthAngle )
{
    // Compute first derivative of the azimuth angle w.r.t. time.
    double derivativeAzimuthAngle = computeFirstDerivativeAzimuthAngleWrtTime( currentAzimuthAngle );

    // Compute and return current velocity vector in spherical coordinates.
    return derivativeAzimuthAngle * computeCurrentVelocityParametrizedByAzimuthAngle( currentAzimuthAngle );
}


//! Compute current state vector in spherical coordinates.
Eigen::Vector6d ExposinsShaping::computeStateVectorInSphericalCoordinates( const double currentAzimuthAngle )
{
    Eigen::Vector6d currentSphericalState;
    currentSphericalState.segment( 0, 3 ) = computePositionVectorInSphericalCoordinates( currentAzimuthAngle );
    currentSphericalState.segment( 3, 3 ) = computeVelocityVectorInSphericalCoordinates( currentAzimuthAngle );

    return currentSphericalState;
}

//! Compute current cartesian state.
Eigen::Vector6d ExposinsShaping::computeNormalizedStateVector( const double currentAzimuthAngle )
{
    return coordinate_conversions::convertSphericalToCartesianState( computeStateVectorInSphericalCoordinates( currentAzimuthAngle ) );
}


//! Compute current cartesian state.
Eigen::Vector6d ExposinsShaping::computeCurrentStateVector( const double currentAzimuthAngle )
{
    Eigen::Vector6d dimensionalStateVector;
    dimensionalStateVector.segment( 0, 3 ) = computeNormalizedStateVector( currentAzimuthAngle ).segment( 0, 3 )
            * physical_constants::ASTRONOMICAL_UNIT;
    dimensionalStateVector.segment( 3, 3 ) = computeNormalizedStateVector( currentAzimuthAngle ).segment( 3, 3 )
            * physical_constants::ASTRONOMICAL_UNIT / physical_constants::JULIAN_YEAR;

    return dimensionalStateVector;
}


//! Compute current velocity in spherical coordinates parametrized by azimuth angle theta.
Eigen::Vector3d ExposinsShaping::computeCurrentVelocityParametrizedByAzimuthAngle( const double currentAzimuthAngle )
{

    // Retrieve current radial distance and elevation angle, as well as their derivatives w.r.t. azimuth angle.
    double radialDistance = radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle );
    double derivativeRadialDistance = radialDistanceCompositeFunction_->evaluateCompositeFunctionFirstDerivative( currentAzimuthAngle );
    double elevationAngle = elevationAngleCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle );
    double derivativeElevationAngle = elevationAngleCompositeFunction_->evaluateCompositeFunctionFirstDerivative( currentAzimuthAngle );

    // Compute and return velocity vector parametrized by azimuth angle.
    return ( Eigen::Vector3d() << derivativeRadialDistance,
             radialDistance * std::cos( elevationAngle ),
             radialDistance * derivativeElevationAngle ).finished();
}


//! Compute current acceleration in spherical coordinates parametrized by azimuth angle theta.
Eigen::Vector3d ExposinsShaping::computeCurrentAccelerationParametrizedByAzimuthAngle( const double currentAzimuthAngle )
{
    // Retrieve spherical coordinates and their derivatives w.r.t. to the azimuth angle.
    double radialDistance = radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle );
    double firstDerivativeRadialDistance = radialDistanceCompositeFunction_->evaluateCompositeFunctionFirstDerivative( currentAzimuthAngle );
    double secondDerivativeRadialDistance = radialDistanceCompositeFunction_->evaluateCompositeFunctionSecondDerivative( currentAzimuthAngle );
    double elevationAngle = elevationAngleCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle );
    double firstDerivativeElevationAngle = elevationAngleCompositeFunction_->evaluateCompositeFunctionFirstDerivative( currentAzimuthAngle );
    double secondDerivativeElevationAngle = elevationAngleCompositeFunction_->evaluateCompositeFunctionSecondDerivative( currentAzimuthAngle );

    // Compute and return acceleration vector parametrized by the azimuth angle theta.
    Eigen::Vector3d accelerationParametrizedByAzimuthAngle;
    accelerationParametrizedByAzimuthAngle[ 0 ] = secondDerivativeRadialDistance
            - radialDistance * ( std::pow( firstDerivativeElevationAngle, 2.0 ) + std::pow( std::cos( elevationAngle ), 2.0 ) );
    accelerationParametrizedByAzimuthAngle[ 1 ] = 2.0 * firstDerivativeRadialDistance * std::cos( elevationAngle )
            - 2.0 * radialDistance * firstDerivativeElevationAngle * std::sin( elevationAngle );
    accelerationParametrizedByAzimuthAngle[ 2 ] = 2.0 * firstDerivativeRadialDistance * firstDerivativeElevationAngle
            + radialDistance * ( secondDerivativeElevationAngle + std::sin( elevationAngle ) * std::cos( elevationAngle ) );

    return accelerationParametrizedByAzimuthAngle;

}

//! Compute thrust acceleration vector in spherical coordinates.
Eigen::Vector3d ExposinsShaping::computeThrustAccelerationInSphericalCoordinates( const double currentAzimuthAngle )
{
    // Compute current radial distance.
    double radialDistance = radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle );

    // Compute first and second derivatives of the azimuth angle w.r.t. time.
    double firstDerivativeAzimuthAngleWrtTime = computeFirstDerivativeAzimuthAngleWrtTime( currentAzimuthAngle );
    double secondDerivativeAzimuthAngleWrtTime = computeSecondDerivativeAzimuthAngleWrtTime( currentAzimuthAngle );

    // Compute velocity vector parametrized by azimuth angle theta.
    Eigen::Vector3d velocityParametrizedByAzimuthAngle = computeCurrentVelocityParametrizedByAzimuthAngle( currentAzimuthAngle );

    // Compute acceleration vector parametrized by azimuth angle theta.
    Eigen::Vector3d accelerationParametrizedByAzimuthAngle = computeCurrentAccelerationParametrizedByAzimuthAngle( currentAzimuthAngle );


    // Compute and return the current thrust acceleration vector in spherical coordinates.
    return std::pow( firstDerivativeAzimuthAngleWrtTime, 2.0 ) * accelerationParametrizedByAzimuthAngle
            + secondDerivativeAzimuthAngleWrtTime * velocityParametrizedByAzimuthAngle
            + centralBodyGravitationalParameter_ / std::pow( radialDistance, 3.0 ) * ( Eigen::Vector3d() << radialDistance, 0.0, 0.0 ).finished();

}

//! Compute current thrust acceleration in cartesian coordinates.
Eigen::Vector3d ExposinsShaping::computeNormalizedThrustAccelerationVector( const double currentAzimuthAngle )
{
    Eigen::Vector3d cartesianAcceleration;

    Eigen::Vector6d sphericalStateToBeConverted;
    sphericalStateToBeConverted.segment( 0, 3 ) = computePositionVectorInSphericalCoordinates( currentAzimuthAngle );
    sphericalStateToBeConverted.segment( 3, 3 ) = computeThrustAccelerationInSphericalCoordinates( currentAzimuthAngle );

    cartesianAcceleration = coordinate_conversions::convertSphericalToCartesianState( sphericalStateToBeConverted ).segment( 3, 3 );

    return cartesianAcceleration;
}

//! Compute magnitude cartesian acceleration.
double  ExposinsShaping::computeCurrentThrustAccelerationMagnitude( double currentAzimuthAngle )
{
    return computeNormalizedThrustAccelerationVector( currentAzimuthAngle ).norm() * physical_constants::ASTRONOMICAL_UNIT
            / std::pow( physical_constants::JULIAN_YEAR, 2.0 );
}

//! Compute direction thrust acceleration in cartesian coordinates.
Eigen::Vector3d ExposinsShaping::computeCurrentThrustAccelerationDirection( double currentAzimuthAngle )
{
    return computeNormalizedThrustAccelerationVector( currentAzimuthAngle ).normalized();
}

//! Compute Normalized thrust acceleration magnitude - Exposins
double  ExposinsShaping::computeCurrentNormalizedThrustAccelerationMagnitude( double currentAzimuthAngle )
{


    double windTravelProduct = windingParameter_*travelledAzimuthAngle_;
    double logRadiiFraction = std::log(initialCylindricalRadius_/finalCylindricalRadius_);

    double dynamicRangeValue = std::sqrt(std::pow((logRadiiFraction + (std::tan(requiredGamma_)/windingParameter_)*std::sin(windTravelProduct))/(1-std::cos(windTravelProduct)),2) + std::pow(std::tan(requiredGamma_),2)/std::pow(windingParameter_,2));
    double dynamicRangeSign  = (logRadiiFraction + (std::tan(requiredGamma_)/windingParameter_)*std::sin(windTravelProduct));
    double dynamicRangeParameter = std::copysign(dynamicRangeValue,dynamicRangeSign); // possible problems with zero

    double phaseAngle = std::acos(std::tan(requiredGamma_)/(windingParameter_*dynamicRangeParameter));

    double scalingFactor = initialCylindricalRadius_/(std::exp(dynamicRangeParameter*std::sin(phaseAngle))); // Unit is meter [m] if multiplied with physical_constants::ASTRONOMICAL_UNIT*

    double flightAngle = std::atan(dynamicRangeParameter*windingParameter_* std::cos(windingParameter_*currentAzimuthAngle+phaseAngle)); // diff cosine vamue

    double tanGamma = dynamicRangeParameter*windingParameter_*std::cos(windingParameter_*currentAzimuthAngle + phaseAngle);
    double k1k22s   = dynamicRangeParameter*std::pow(windingParameter_,2)*std::sin(windingParameter_*currentAzimuthAngle + phaseAngle);

    double currentRadius = scalingFactor*std::exp(dynamicRangeParameter*std::sin(windingParameter_*currentAzimuthAngle + phaseAngle)) * physical_constants::ASTRONOMICAL_UNIT;
    double centralBodyGravitationalParameter = centralBodyGravitationalParameter_ / std::pow( physical_constants::JULIAN_YEAR, 2.0 )
            * std::pow( physical_constants::ASTRONOMICAL_UNIT, 3.0 );

    double C1 = tanGamma/(2*std::cos(flightAngle));
    double C2 = 1;
    double C3 = std::pow(tanGamma,2) + k1k22s + 1;
    double C4 = std::pow(windingParameter_,2)*(1-2*dynamicRangeParameter*std::sin(windingParameter_*currentAzimuthAngle + phaseAngle));
    double C5 = std::pow(C3,2);
    double C6 = centralBodyGravitationalParameter/std::pow(currentRadius,2);


//            std::cout << "requiredGamma " << requiredGamma_ << std::endl;
//            std::cout << "dynamicRangeValue " << dynamicRangeValue << std::endl;
//            std::cout << "dynamicRangeSign " << dynamicRangeSign << std::endl;
//            std::cout << "dynamicRangeParameter " << dynamicRangeParameter << std::endl;
//            std::cout << "phaseAngle " << phaseAngle << std::endl;
//            std::cout << "scalingFactor " << scalingFactor << std::endl;

//            std::cout << "currentRadius " << currentRadius << std::endl;
//            std::cout << "tanGamma " << tanGamma << std::endl;


//    return (tanGamma/(2*std::cos(flightAngle)))*((1/(std::pow(tanGamma,2)+k1k22s+1)) - ((std::pow(windingParameter_,2)-2*k1k22s)/std::pow(std::pow(tanGamma,2)+k1k22s+1,2)));
    return (C1*(C2/C3 - C4/C5))*C6;

}

//! Compute first derivative Azimuth
double  ExposinsShaping::computeFirstDerivativeAzimuthAngleWrtTimeExposins(const double currentAzimuthAngle )
{


    double windTravelProduct = windingParameter_*travelledAzimuthAngle_;
    double logRadiiFraction = std::log(initialCylindricalRadius_/finalCylindricalRadius_);

    double dynamicRangeValue = std::sqrt(std::pow((logRadiiFraction + (std::tan(requiredGamma_)/windingParameter_)*std::sin(windTravelProduct))/(1-std::cos(windTravelProduct)),2) + std::pow(std::tan(requiredGamma_),2)/std::pow(windingParameter_,2));
    double dynamicRangeSign  = (logRadiiFraction + (std::tan(requiredGamma_)/windingParameter_)*std::sin(windTravelProduct));
    double dynamicRangeParameter = std::copysign(dynamicRangeValue,dynamicRangeSign); // possible problems with zero

    double phaseAngle = std::acos(std::tan(requiredGamma_)/(windingParameter_*dynamicRangeParameter));

    double scalingFactor = initialCylindricalRadius_/(std::exp(dynamicRangeParameter*std::sin(phaseAngle))); // Unit is meter [m] if multiplied with physical_constants::ASTRONOMICAL_UNIT*

    double tanGamma = dynamicRangeParameter*windingParameter_*std::cos(windingParameter_*currentAzimuthAngle + phaseAngle);
    double k1k22s   = dynamicRangeParameter*std::pow(windingParameter_,2)*std::sin(windingParameter_*currentAzimuthAngle + phaseAngle);
    double currentRadius = scalingFactor*std::exp(dynamicRangeParameter*std::sin(windingParameter_*currentAzimuthAngle + phaseAngle)) * physical_constants::ASTRONOMICAL_UNIT;
    double centralBodyGravitationalParameter = centralBodyGravitationalParameter_ / std::pow( physical_constants::JULIAN_YEAR, 2.0 )
            * std::pow( physical_constants::ASTRONOMICAL_UNIT, 3.0 );

//        std::cout << "requiredGamma " << requiredGamma_ << std::endl;
//        std::cout << "dynamicRangeValue " << dynamicRangeValue << std::endl;
//        std::cout << "dynamicRangeSign " << dynamicRangeSign << std::endl;
//        std::cout << "dynamicRangeParameter " << dynamicRangeParameter << std::endl;
//        std::cout << "phaseAngle " << phaseAngle << std::endl;
//        std::cout << "scalingFactor " << scalingFactor << std::endl;

//        std::cout << "currentRadius " << currentRadius*physical_constants::ASTRONOMICAL_UNIT << std::endl;

//        std::cout << "std::pow(tanGamma,2) " << std::pow(tanGamma,2) << std::endl;
//        std::cout << "k1k22s " << k1k22s <<std::endl;
//        std::cout << "centralBodyGravitationalParameter_ " << centralBodyGravitationalParameter_ <<std::endl;
//        std::cout << "pow(currentRadius,3) " << pow(currentRadius,3) <<std::endl;



//        std::cout << "result " << (centralBodyGravitationalParameter_/std::pow(currentRadius,3))*1/(std::pow(tanGamma,2) + k1k22s + 1) << std::endl;

    return std::sqrt((centralBodyGravitationalParameter/std::pow(currentRadius,3))*1/(std::pow(tanGamma,2) + k1k22s + 1));

}


//! Compute final deltaV.
double ExposinsShaping::computeDeltaV( )
{
    // Define the derivative of the deltaV, ie thrust acceleration function, as a function of the azimuth angle.
    std::function< double( const double ) > derivativeFunctionDeltaV = [ = ] ( const double currentAzimuthAngle ){

        double thrustAcceleration = computeThrustAccelerationInSphericalCoordinates( currentAzimuthAngle ).norm()
                * std::sqrt( computeScalarFunctionTimeEquation( currentAzimuthAngle )
                             * std::pow( radialDistanceCompositeFunction_->evaluateCompositeFunction( currentAzimuthAngle ), 2.0 )
                             / centralBodyGravitationalParameter_ );

        return thrustAcceleration;

    };

    // Define numerical quadrature from quadratrure settings.
    std::shared_ptr< numerical_quadrature::NumericalQuadrature< double, double > > quadrature =
            numerical_quadrature::createQuadrature( derivativeFunctionDeltaV, quadratureSettings_, finalAzimuthAngle_ );

    // Return dimensional deltaV
    return quadrature->getQuadrature( ) * physical_constants::ASTRONOMICAL_UNIT / physical_constants::JULIAN_YEAR;
}

//! Compute final deltaV.
double ExposinsShaping::computeDeltaVExposins( )
{
    // Define the derivative of the deltaV, ie thrust acceleration function, as a function of the azimuth angle.
    std::function< double( const double ) > derivativeFunctionDeltaV = [ = ] ( const double currentAzimuthAngle ){

        return computeCurrentNormalizedThrustAccelerationMagnitude( currentAzimuthAngle )/
                computeFirstDerivativeAzimuthAngleWrtTimeExposins( currentAzimuthAngle );

    };

    // Define numerical quadrature from quadratrure settings.
    std::shared_ptr< numerical_quadrature::NumericalQuadrature< double, double > > quadrature =
            numerical_quadrature::createQuadrature( derivativeFunctionDeltaV, quadratureSettings_, travelledAzimuthAngle_ );

    // Return dimensional deltaV
    return quadrature->getQuadrature( );
}




////! Get low-thrust acceleration model from shaping method.
//std::shared_ptr< propulsion::ThrustAcceleration > ExposinsShaping::getLowThrustAccelerationModel(
////        simulation_setup::NamedBodyMap& bodyMap,
////        const std::string& bodyToPropagate,
//        std::function< double( const double ) > specificImpulseFunction,
//        std::shared_ptr< interpolators::OneDimensionalInterpolator< double, double > > interpolatorPolarAngleFromTime )
//{

//    std::shared_ptr< simulation_setup::Body > vehicle = bodyMap_[ bodyToPropagate_ ];

//    // Define thrust magnitude function from the shaped trajectory.
//    std::function< double( const double ) > thrustMagnitudeFunction = [ = ]( const double currentTime )
//    {

//        // Compute current azimuth angle.
//        double currentAzimuthAngle = interpolatorPolarAngleFromTime->interpolate( currentTime );

//        // Compute current acceleration.
//        double currentAcceleration = computeCurrentThrustAccelerationMagnitude( currentAzimuthAngle ) * physical_constants::ASTRONOMICAL_UNIT
//                / std::pow( physical_constants::JULIAN_YEAR, 2.0 );

//        // Compute current mass of the vehicle.
//        double currentMass = vehicle->getBodyMass();

//        // Compute and return magnitude of the low-thrust force.
//        return currentAcceleration * currentMass;
//    };

//    // Define thrust magnitude settings from thrust magnitude function.
//    std::shared_ptr< simulation_setup::FromFunctionThrustMagnitudeSettings > thrustMagnitudeSettings
//            = std::make_shared< simulation_setup::FromFunctionThrustMagnitudeSettings >(
//                thrustMagnitudeFunction, specificImpulseFunction );


//    // Define thrust direction function from the shaped trajectory.
//    std::function< Eigen::Vector3d( const double ) > thrustDirectionFunction = [ = ]( const double currentTime )
//    {
//        // Compute current azimuth angle.
//        double currentAzimuthAngle = interpolatorPolarAngleFromTime->interpolate( currentTime );

//        // Compute current direction of the acceleration vector.
//        Eigen::Vector3d currentAccelerationDirection = computeCurrentThrustAccelerationDirection( currentAzimuthAngle );

//        // Return direction of the low-thrust acceleration.
//        return currentAccelerationDirection;
//    };

//    // Define thrust direction settings from the direction of thrust acceleration retrieved from the shaping method.
//    std::shared_ptr< simulation_setup::CustomThrustDirectionSettings > thrustDirectionSettings =
//            std::make_shared< simulation_setup::CustomThrustDirectionSettings >( thrustDirectionFunction );

//    // Define thrust acceleration settings.
//    std::shared_ptr< simulation_setup::ThrustAccelerationSettings > thrustAccelerationSettings =
//            std::make_shared< simulation_setup::ThrustAccelerationSettings >(
//                thrustDirectionSettings, thrustMagnitudeSettings );

//    // Create low thrust acceleration model.
//    std::shared_ptr< propulsion::ThrustAcceleration > lowThrustAccelerationModel = createThrustAcceleratioModel(
//                thrustAccelerationSettings, bodyMap_, bodyToPropagate_ );

//    return lowThrustAccelerationModel;
//}


void ExposinsShaping::computeShapedTrajectoryAndFullPropagation(
//        simulation_setup::NamedBodyMap& bodyMap,
        std::function< double( const double ) > specificImpulseFunction,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > > integratorSettings,
        std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
                std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > >& propagatorSettings,
        std::map< double, Eigen::VectorXd >& fullPropagationResults,
        std::map< double, Eigen::VectorXd >& shapingMethodResults,
        std::map< double, Eigen::VectorXd >& dependentVariablesHistory,
        const bool isMassPropagated ){

    fullPropagationResults.clear();
    shapingMethodResults.clear();
    dependentVariablesHistory.clear();

    std::string bodyToPropagate = propagatorSettings.first->bodiesToIntegrate_[ 0 ];

    // Retrieve initial step size.
    double initialStepSize = integratorSettings->initialTimeStep_;

    // Vector of azimuth angles at which the time should be computed.
    Eigen::VectorXd azimuthAnglesToComputeAssociatedEpochs =
            Eigen::VectorXd::LinSpaced( std::ceil( computeNormalizedTimeOfFlight() * physical_constants::JULIAN_YEAR / initialStepSize ),
                                        initialAzimuthAngle_, finalAzimuthAngle_ );

    std::map< double, double > dataToInterpolate;
    for ( int i = 0 ; i < azimuthAnglesToComputeAssociatedEpochs.size() ; i++ )
    {
        dataToInterpolate[ computeCurrentTimeFromAzimuthAngle( azimuthAnglesToComputeAssociatedEpochs[ i ] ) * physical_constants::JULIAN_YEAR ]
                = azimuthAnglesToComputeAssociatedEpochs[ i ];
    }

    // Create interpolator.
    std::shared_ptr< interpolators::InterpolatorSettings > interpolatorSettings =
            std::make_shared< interpolators::LagrangeInterpolatorSettings >( 10 );

    std::shared_ptr< interpolators::OneDimensionalInterpolator< double, double > > interpolator =
            interpolators::createOneDimensionalInterpolator( dataToInterpolate, interpolatorSettings );

    // Compute halved time of flight.
    double halvedTimeOfFlight = computeNormalizedTimeOfFlight() / 2.0;

    // Compute azimuth angle at half of the time of flight.
    double azimuthAngleAtHalvedTimeOfFlight = interpolator->interpolate( halvedTimeOfFlight * physical_constants::JULIAN_YEAR );

    // Compute state at half of the time of flight.
    Eigen::Vector6d initialStateAtHalvedTimeOfFlight = computeNormalizedStateVector( azimuthAngleAtHalvedTimeOfFlight );
    initialStateAtHalvedTimeOfFlight.segment( 0, 3 ) *= physical_constants::ASTRONOMICAL_UNIT;
    initialStateAtHalvedTimeOfFlight.segment( 3, 3 ) *= physical_constants::ASTRONOMICAL_UNIT / physical_constants::JULIAN_YEAR;

    // Create low thrust acceleration model.
    std::shared_ptr< propulsion::ThrustAcceleration > lowThrustAccelerationModel =
            getLowThrustAccelerationModel( /*bodyMap, propagatorSettings.first->bodiesToIntegrate_[ 0 ],*/ specificImpulseFunction/*,
                                           std::bind( &ExposinsShaping::computeCurrentThrustAccelerationDirection, this, std::placeholders::_1 ),
                                           std::bind( &ExposinsShaping::computeCurrentThrustAccelerationMagnitude, this, std::placeholders::_1 )*/ /*, interpolator*/ );

    basic_astrodynamics::AccelerationMap accelerationMap = propagators::getAccelerationMapFromPropagatorSettings(
                std::dynamic_pointer_cast< propagators::SingleArcPropagatorSettings< double > >( propagatorSettings.first ) );

    accelerationMap[ propagatorSettings.first->bodiesToIntegrate_[ 0 ] ][ propagatorSettings.first->bodiesToIntegrate_[ 0 ] ]
            .push_back( lowThrustAccelerationModel );


    // Create complete propagation settings (backward and forward propagations).
    std::pair< std::shared_ptr< propagators::PropagatorSettings< double > >,
            std::shared_ptr< propagators::PropagatorSettings< double > > > completePropagatorSettings;


    // Define translational state propagation settings
    std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
            std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > translationalStatePropagatorSettings;

    // Define backward translational state propagation settings.
    translationalStatePropagatorSettings.first = std::make_shared< propagators::TranslationalStatePropagatorSettings< double > >
                        ( propagatorSettings.first->centralBodies_, accelerationMap, propagatorSettings.first->bodiesToIntegrate_,
                          initialStateAtHalvedTimeOfFlight, propagatorSettings.first->getTerminationSettings(),
                          propagatorSettings.first->propagator_, propagatorSettings.first->getDependentVariablesToSave() );

    // Define forward translational state propagation settings.
    translationalStatePropagatorSettings.second = std::make_shared< propagators::TranslationalStatePropagatorSettings< double > >
                        ( propagatorSettings.second->centralBodies_, accelerationMap, propagatorSettings.second->bodiesToIntegrate_,
                          initialStateAtHalvedTimeOfFlight, propagatorSettings.second->getTerminationSettings(),
                          propagatorSettings.second->propagator_, propagatorSettings.second->getDependentVariablesToSave() );


    // If translational state and mass are propagated concurrently.
    if ( isMassPropagated )
    {
        // Create mass rate models
        std::map< std::string, std::shared_ptr< basic_astrodynamics::MassRateModel > > massRateModels;
        massRateModels[ bodyToPropagate ] = createMassRateModel( bodyToPropagate, std::make_shared< simulation_setup::FromThrustMassModelSettings >( 1 ),
                                                           bodyMap_, accelerationMap );


        // Propagate mass until half of the time of flight.
        std::shared_ptr< propagators::PropagatorSettings< double > > massPropagatorSettingsToHalvedTimeOfFlight =
                std::make_shared< propagators::MassPropagatorSettings< double > >( std::vector< std::string >{ bodyToPropagate }, massRateModels,
                    ( Eigen::Vector1d() << bodyMap_[ bodyToPropagate ]->getBodyMass() ).finished(),
                    std::make_shared< propagators::PropagationTimeTerminationSettings >( halvedTimeOfFlight * physical_constants::JULIAN_YEAR, true ) );

        integratorSettings->initialTime_ = 0.0;

        // Create dynamics simulation object.
        propagators::SingleArcDynamicsSimulator< double, double > dynamicsSimulator(
                    bodyMap_, integratorSettings, massPropagatorSettingsToHalvedTimeOfFlight, true, false, false );

        // Propagate spacecraft mass until half of the time of flight.
        std::map< double, Eigen::VectorXd > propagatedMass = dynamicsSimulator.getEquationsOfMotionNumericalSolution( );
        double massAtHalvedTimeOfFlight = propagatedMass.rbegin()->second[ 0 ];

        // Create settings for propagating the mass of the vehicle.
        std::pair< std::shared_ptr< propagators::MassPropagatorSettings< double > >,
                std::shared_ptr< propagators::MassPropagatorSettings< double > > > massPropagatorSettings;

        // Define backward mass propagation settings.
        massPropagatorSettings.first = std::make_shared< propagators::MassPropagatorSettings< double > >(
                    std::vector< std::string >{ bodyToPropagate }, massRateModels, ( Eigen::Matrix< double, 1, 1 >( ) << massAtHalvedTimeOfFlight ).finished( ),
                                                                      propagatorSettings.first->getTerminationSettings() );

        // Define forward mass propagation settings.
        massPropagatorSettings.second = std::make_shared< propagators::MassPropagatorSettings< double > >(
                    std::vector< std::string >{ bodyToPropagate }, massRateModels, ( Eigen::Matrix< double, 1, 1 >( ) << massAtHalvedTimeOfFlight ).finished( ),
                                                                      propagatorSettings.second->getTerminationSettings() );


        // Create list of propagation settings.
        std::pair< std::vector< std::shared_ptr< propagators::SingleArcPropagatorSettings< double > > >,
                std::vector< std::shared_ptr< propagators::SingleArcPropagatorSettings< double > > > > propagatorSettingsVector;

        // Backward propagator settings vector.
        propagatorSettingsVector.first.push_back( translationalStatePropagatorSettings.first );
        propagatorSettingsVector.first.push_back( massPropagatorSettings.first );

        // Forward propagator settings vector.
        propagatorSettingsVector.second.push_back( translationalStatePropagatorSettings.second );
        propagatorSettingsVector.second.push_back( massPropagatorSettings.second );


        // Backward hybrid propagation settings.
        completePropagatorSettings.first = std::make_shared< propagators::MultiTypePropagatorSettings< double > >( propagatorSettingsVector.first,
                    propagatorSettings.first->getTerminationSettings(), propagatorSettings.first->getDependentVariablesToSave() );

        // Forward hybrid propagation settings.
        completePropagatorSettings.second = std::make_shared< propagators::MultiTypePropagatorSettings< double > >( propagatorSettingsVector.second,
                    propagatorSettings.second->getTerminationSettings(), propagatorSettings.second->getDependentVariablesToSave() );


    }

    // If only translational state is propagated.
    else
    {
        // Backward hybrid propagation settings.
        completePropagatorSettings.first = translationalStatePropagatorSettings.first;

        // Forward hybrid propagation settings.
        completePropagatorSettings.second =  translationalStatePropagatorSettings.second;
    }


    // Define forward propagator settings variables.
    integratorSettings->initialTime_ = halvedTimeOfFlight * physical_constants::JULIAN_YEAR;

//    // Define propagation settings
//    std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > propagatorSettingsForwardPropagation;
//    std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > propagatorSettingsBackwardPropagation;

//    // Define forward propagation settings.
//    propagatorSettingsForwardPropagation = std::make_shared< propagators::TranslationalStatePropagatorSettings< double > >
//                        ( propagatorSettings.second->centralBodies_ /*centralBodies*/, accelerationMap, propagatorSettings.second->bodiesToIntegrate_ /*bodiesToPropagate*/,
//                          initialStateAtHalvedTimeOfFlight, propagatorSettings.second->getTerminationSettings() /*terminationSettings.second*/,
//                          propagatorSettings.second->propagator_ /*propagatorType*/, propagatorSettings.second->getDependentVariablesToSave() /*dependentVariablesToSave*/ );

//    // Define backward propagation settings.
//    propagatorSettingsBackwardPropagation = std::make_shared< propagators::TranslationalStatePropagatorSettings< double > >
//                        ( propagatorSettings.first->centralBodies_ /*centralBodies*/, accelerationMap, propagatorSettings.first->bodiesToIntegrate_ /*bodiesToPropagate*/,
//                          initialStateAtHalvedTimeOfFlight, propagatorSettings.first->getTerminationSettings() /*terminationSettings.first*/,
//                          propagatorSettings.first->propagator_ /*propagatorType*/, propagatorSettings.first->getDependentVariablesToSave() /*dependentVariablesToSave*/ );

    // Perform forward propagation.
    propagators::SingleArcDynamicsSimulator< > dynamicsSimulatorIntegrationForwards( bodyMap_, integratorSettings, completePropagatorSettings.second );
    std::map< double, Eigen::VectorXd > stateHistoryFullProblemForwardPropagation = dynamicsSimulatorIntegrationForwards.getEquationsOfMotionNumericalSolution( );
    std::map< double, Eigen::VectorXd > dependentVariableHistoryForwardPropagation = dynamicsSimulatorIntegrationForwards.getDependentVariableHistory( );

    // Compute and save full propagation and shaping method results along the forward propagation direction.
    for( std::map< double, Eigen::VectorXd >::iterator itr = stateHistoryFullProblemForwardPropagation.begin( );
         itr != stateHistoryFullProblemForwardPropagation.end( ); itr++ )
    {
        double currentAzimuthAngle = interpolator->interpolate( itr->first );

        Eigen::Vector6d currentNormalisedState = computeNormalizedStateVector( currentAzimuthAngle );
        Eigen::Vector6d currentState;
        currentState.segment( 0, 3 ) = currentNormalisedState.segment( 0, 3 ) * physical_constants::ASTRONOMICAL_UNIT;
        currentState.segment( 3, 3 ) = currentNormalisedState.segment( 3, 3 ) * physical_constants::ASTRONOMICAL_UNIT / physical_constants::JULIAN_YEAR;
        shapingMethodResults[ itr->first ] = currentState;
        fullPropagationResults[ itr->first ] = itr->second;
        dependentVariablesHistory[ itr->first ] = dependentVariableHistoryForwardPropagation[ itr->first ];
    }


    // Define backward propagator settings variables.
    integratorSettings->initialTimeStep_ = - integratorSettings->initialTimeStep_;
    integratorSettings->initialTime_ = halvedTimeOfFlight * physical_constants::JULIAN_YEAR;

    // Perform the backward propagation.
    propagators::SingleArcDynamicsSimulator< > dynamicsSimulatorIntegrationBackwards( bodyMap_, integratorSettings, completePropagatorSettings.first );
    std::map< double, Eigen::VectorXd > stateHistoryFullProblemBackwardPropagation = dynamicsSimulatorIntegrationBackwards.getEquationsOfMotionNumericalSolution( );
    std::map< double, Eigen::VectorXd > dependentVariableHistoryBackwardPropagation = dynamicsSimulatorIntegrationBackwards.getDependentVariableHistory( );

    // Compute and save full propagation and shaping method results along the backward propagation direction
    for( std::map< double, Eigen::VectorXd >::iterator itr = stateHistoryFullProblemBackwardPropagation.begin( );
         itr != stateHistoryFullProblemBackwardPropagation.end( ); itr++ )
    {
        double currentAzimuthAngle = interpolator->interpolate( itr->first );

        Eigen::Vector6d currentNormalisedState = computeNormalizedStateVector( currentAzimuthAngle );
        Eigen::Vector6d currentState;
        currentState.segment( 0, 3 ) = currentNormalisedState.segment( 0, 3 ) * physical_constants::ASTRONOMICAL_UNIT;
        currentState.segment( 3, 3 ) = currentNormalisedState.segment( 3, 3 ) * physical_constants::ASTRONOMICAL_UNIT / physical_constants::JULIAN_YEAR;
        shapingMethodResults[ itr->first ] = currentState;
        fullPropagationResults[ itr->first ] = itr->second;
        dependentVariablesHistory[ itr->first ] = dependentVariableHistoryBackwardPropagation[ itr->first ];
    }

    // Reset initial integrator settings.
    integratorSettings->initialTimeStep_ = - integratorSettings->initialTimeStep_;

}


} // namespace shape_based_methods
} // namespace tudat
