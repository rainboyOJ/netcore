#pragma once
#include "http_client.hpp"

namespace netcore {
    
	class client_factory {
	public:
		static client_factory& instance() {
			static client_factory instance;
			return instance;
		}

		template<typename...Args>
		auto new_client(Args&&... args) {
			return std::make_shared<http_client>(std::forward<Args>(args)...);
		}

	private:
		client_factory() {}
		~client_factory() {}

		client_factory(const client_factory&) = delete;
		client_factory& operator=(const client_factory&) = delete;
		client_factory(client_factory&&) = delete;
		client_factory& operator=(client_factory&&) = delete;

	};
} // end namespace netcore

