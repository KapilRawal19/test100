/****************************************************************************
 *
 *   Copyright (c) 2017 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file Functions.hpp
 *
 * collection of rather simple mathematical functions that get used over and over again
 */

#pragma once

#include "Limits.hpp"

namespace math
{

// Type-safe signum function
template<typename T>
int sign(T val)
{
	return (T(0) < val) - (val < T(0));
}

// Type-safe signum function with zero treated as positive
template<typename T>
int signNoZero(T val)
{
	return (T(0) <= val) - (val < T(0));
}

/*
 * So called exponential curve function implementation.
 * It is essentially a linear combination between a linear and a cubic function.
 * @param value [-1,1] input value to function
 * @param e [0,1] function parameter to set ratio between linear and cubic shape
 * 		0 - pure linear function
 * 		1 - pure cubic function
 * @return result of function output
 */
template<typename T>
const T expo(const T &value, const T &e)
{
	T x = constrain(value, (T) - 1, (T) 1);
	T ec = constrain(e, (T) 0, (T) 1);
	return (1 - ec) * x + ec * x * x * x;
}

/*
 * So called SuperExpo function implementation.
 * It is a 1/(1-x) function to further shape the rc input curve intuitively.
 * I enhanced it compared to other implementations to keep the scale between [-1,1].
 * @param value [-1,1] input value to function
 * @param e [0,1] function parameter to set ratio between linear and cubic shape (see expo)
 * @param g [0,1) function parameter to set SuperExpo shape
 * 		0 - pure expo function
 * 		0.99 - very strong bent curve, stays zero until maximum stick input
 * @return result of function output
 */
template<typename T>
const T superexpo(const T &value, const T &e, const T &g)
{
	T x = constrain(value, (T) - 1, (T) 1);
	T gc = constrain(g, (T) 0, (T) 0.99);
	return expo(x, e) * (1 - gc) / (1 - fabsf(x) * gc);
}

/*
 * Deadzone function being linear and continuous outside of the deadzone
 * 1                ------
 *                /
 *             --
 *           /
 * -1 ------
 *        -1 -dz +dz 1
 * @param value [-1,1] input value to function
 * @param dz [0,1) ratio between deazone and complete span
 * 		0 - no deadzone, linear -1 to 1
 * 		0.5 - deadzone is half of the span [-0.5,0.5]
 * 		0.99 - almost entire span is deadzone
 */
template<typename T>
const T deadzone(const T &value, const T &dz)
{
	T x = constrain(value, (T) - 1, (T) 1);
	T dzc = constrain(dz, (T) 0, (T) 0.99);
	// Rescale the input such that we get a piecewise linear function that will be continuous with applied deadzone
	T out = (x - sign(x) * dzc) / (1 - dzc);
	// apply the deadzone (values zero around the middle)
	return out * (fabsf(x) > dzc);
}

template<typename T>
const T expo_deadzone(const T &value, const T &e, const T &dz)
{
	return expo(deadzone(value, dz), e);
}


/*
 * Constant, linear, constant function with the two corner points as parameters
 * y_high          -------
 *                /
 *               /
 *              /
 * y_low -------
 *         x_low   x_high
 */
template<typename T>
const T gradual(const T &value, const T &x_low, const T &x_high, const T &y_low, const T &y_high)
{
	if (value < x_low) {
		return y_low;

	} else if (value > x_high) {
		return y_high;

	} else {
		/* linear function between the two points */
		T a = (y_high - y_low) / (x_high - x_low);
		T b = y_low - a * x_low;
		return  a * value + b;
	}
}

/*
 * Exponential function of the form Y_out = a*b^X + c
 *
 * Y_max    |   *
 *          |    *
 *          |      *
 *          |        *
 *          |           *
 * Y_middle |               *
 *          |                    *
 * Y_min    |                          *    *
 *          | __________________________________
 *              0           1               2
 *
 *
 * @param X in the range [0,2]
 * @param Y_min minimum output at X = 2
 * @param Y_mid middle output at X = 1
 * @param Y_max maximum output at X = 0
 */
template<typename T>
const T expontialFromLimits(const T &X_in, const T &Y_min, const T &Y_mid, const T &Y_max)
{
	const T delta = (T)0.001;
	// constrain X_in to the range of 0 and 2
	T X = math::constrain(X_in, (T)0, (T)2);
	// If Y_mid is exactly in the middle, then just apply linear approach.
	bool use_linear_approach = false;

	if (((Y_max + Y_min) * (T)0.5) - Y_mid < delta) {
		use_linear_approach = true;
	}

	T Y_out;

	if (use_linear_approach) {
		// Y_out =  m*x+q
		float slope = -(Y_max - Y_min) / (T)2.0;
		Y_out = slope * X + Y_max;

	} else {
		// Y_out = a*b^X + c with constraints Y_max = 0, Y_middle = 1, Y_min = 2
		T a = -((Y_mid - Y_max) * (Y_mid - Y_max))
		      / ((T)2.0 * Y_mid - Y_max - Y_min);
		T c = Y_max - a;
		T b = (Y_mid - c) / a;
		Y_out = a * powf(b, X) + c;
	}

	// Y_out needs to be in between max and min
	return constrain(Y_out, Y_min, Y_max);

}
} /* namespace math */