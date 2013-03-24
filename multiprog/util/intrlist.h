///////////////////////////////////////////////////////////////////////////////
//                           The act-o Library                               //
//---------------------------------------------------------------------------//
// Copyright © 2007 - 2012                                                   //
//     Pavel A. Artemkin (acto.stan@gmail.com)                               //
// ------------------------------------------------------------------ -------//
// License:                                                                  //
//     Code covered by the MIT License.                                      //
//     The authors make no representations about the suitability of this     //
//     software for any purpose. It is provided "as is" without express or   //
//     implied warranty.                                                     //
///////////////////////////////////////////////////////////////////////////////

#ifndef intrlist_h_205186795EC84c8cBE78E2C47CF69AC1
#define intrlist_h_205186795EC84c8cBE78E2C47CF69AC1

namespace acto {

namespace core {

	/** 
	 * Интрузивный элемент списка 
	 */
	template <typename T>
	class intrusive_t {
	public:
		intrusive_t()
			: next(0)
		{
		}

		T*  next;
	};

} // namespace core

} // namespace acto

#endif // intrlist_h_205186795EC84c8cBE78E2C47CF69AC1
