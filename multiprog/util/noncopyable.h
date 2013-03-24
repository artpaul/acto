#pragma once

namespace acto {
	class non_copyable_t {
	protected:
		non_copyable_t() {
		}

		~non_copyable_t() {
		}

	private:
		non_copyable_t(const non_copyable_t&);

		const non_copyable_t& operator = (const non_copyable_t&);
	};

} // namespace acto