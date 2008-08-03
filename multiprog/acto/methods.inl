

///////////////////////////////////////////////////////////////////////////////////////////////////
//                        РЕАЛИЗАЦИЯ ВСТРАИВАЕМЫХ И ШАБЛОННЫХ МЕТОДОВ                            //
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace multiprog {

namespace acto {

// Desc: 
inline core::object_t* dereference(object_t& object) {
	return object.m_object;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------	
template <typename ActorT>
	instance_t< ActorT >::instance_t() : object_t() {
		// 1.
		ActorT* const value = new ActorT();
		// 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
		m_object = core::runtime.createActor(value, 0);
		// -
		value->self = actor_t(m_object);
	}
//-------------------------------------------------------------------------------------------------	
template <typename ActorT>
	instance_t< ActorT >::instance_t(actor_t& context) : object_t() {
		// 1.
		ActorT* const value = new ActorT();
		// 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
		m_object = core::runtime.createActor(value, 0);
		// -
		value->context = context;
		value->self    = actor_t(m_object);
	}
//-------------------------------------------------------------------------------------------------
template <typename ActorT>
	instance_t< ActorT >::instance_t(const int options) : object_t() {
		// 1.
		ActorT* const value = new ActorT();
		// 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
		m_object = core::runtime.createActor(value, options);
		// -
		value->self = actor_t(m_object);
	}
//-------------------------------------------------------------------------------------------------
template <typename ActorT>
	instance_t< ActorT >::instance_t(actor_t& context, const int options) {
		// 1.
		ActorT* const value = new ActorT();
		// 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
		m_object = core::runtime.createActor(value, options);
		// -
		value->context = context;
		value->self    = actor_t(m_object);
	}


///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
template <typename ActorT>
	actor_t::actor_t(const instance_t< ActorT >& inst) : object_t( inst ) {
		// -
	}

//-------------------------------------------------------------------------------------------------
template <typename MsgT>
	void actor_t::send(const MsgT& msg) const {
		if (m_object)
			// Отправить сообщение 
            return core::runtime.send(m_object, new MsgT(msg), core::type_box_t< MsgT >());
	}
//-------------------------------------------------------------------------------------------------
template <typename ActorT>
	actor_t& actor_t::operator = (const instance_t< ActorT >& inst) {
		object_t::assign(inst);
		return *this;
	}



namespace core {

///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
template <typename T>
	type_box_t< T >::type_box_t() :
		m_id( core::runtime.typeIdentifier( typeid(T).raw_name() ) )
	{
	}
//-------------------------------------------------------------------------------------------------
template <typename T>
	type_box_t< T >::type_box_t(const type_box_t& rhs) :
		m_id( rhs.m_id )
	{
	}
//-------------------------------------------------------------------------------------------------
template <typename T>
	bool type_box_t< T >::operator == (const value_type& rhs) const {
		return (m_id == rhs);
	}
//-------------------------------------------------------------------------------------------------
template <typename T>
template <typename U>
	bool type_box_t< T >::operator == (const type_box_t< U >& rhs) const {
		return (m_id == rhs.m_id);
	}


///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
template < typename MsgT, typename ClassName >
	inline void base_t::Handler( void (ClassName::*func)(acto::actor_t& sender, const MsgT& msg) ) {
		// Тип сообщения
		type_box_t< MsgT >		        a_type     = type_box_t< MsgT >();
		// Метод, обрабатывающий сообщение
		handler_t< MsgT >::delegate_t	a_delegate = fastdelegate::MakeDelegate(this, func);
		// Обрабочик
		handler_t< MsgT >* const		handler    = new handler_t< MsgT >(a_delegate, a_type);
		
		// Установить обработчик
		set_handler(handler, a_type);
	}
//-------------------------------------------------------------------------------------------------
template < typename MsgT >
	inline void base_t::Handler() {
        // Тип сообщения
		type_box_t< MsgT >	a_type = type_box_t< MsgT >();
		// Сбросить обработчик указанного типа
		set_handler(0, a_type);
	}


///////////////////////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
template <typename MsgT>
	handler_t< MsgT >::handler_t(const delegate_t& delegate_, type_box_t< MsgT >& type_) :
		i_handler ( type_ ),
		// -	
		m_delegate( delegate_ )
	{
	}
//-------------------------------------------------------------------------------------------------
template <typename MsgT>
	void handler_t< MsgT >::invoke(object_t* const sender, msg_t* const msg) {
		m_delegate(acto::actor_t(sender), *static_cast< MsgT* const >(msg));
	}

}; // namespace core

}; // namespace acto

}; // namespace multiprog
