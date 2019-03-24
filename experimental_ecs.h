#include <type_traits>
#include <array>
#include <tuple>
#include <optional>
#include <initializer_list>
#include <utility>
#include <stddef.h>
#include <assert.h>

namespace proto{
    // helper struct - index of given type inside variadic typelist
    // ===============================================================================
    template<typename...> struct typelist_index_of;

    template <typename T, typename... R>
    struct typelist_index_of<T, T, R...> 
        : std::integral_constant<size_t, 0> {};

    template <typename T, typename F, typename... R>
    struct typelist_index_of<T, F, R...> 
        : std::integral_constant<size_t, 1 + typelist_index_of<T,R...>::value> {};
    // ===============================================================================

    // compile time sequence array initialization from isocpp.org
    // ===============================================================================
    template<size_t ...Is>
    constexpr auto make_sequence_impl(std::index_sequence<Is...>){
        return std::index_sequence<Is...>{};
    }
    template<size_t N>
    constexpr auto make_sequence(){
        return make_sequence_impl(std::make_index_sequence<N>{});
    }
    template<size_t ...Is>
    constexpr auto make_array_from_sequence_impl(std::index_sequence<Is...>){
        return std::array<size_t, sizeof...(Is)>{Is...};
    }
    template<typename Seq>
    constexpr auto make_array_from_sequence(Seq){
        return make_array_from_sequence_impl(Seq{});
    }
    // ===============================================================================

    // index with generation, generation is meant to be incremenented
    // every time something new is saved on the given index to prevent errors
    // ===============================================================================
    template<typename T = size_t>
    struct GenIndex{ 
        inline constexpr T index() const { return _index; }
        inline constexpr T gen()   const { return _gen; }
        bool operator==(GenIndex rhs){ return (rhs._index == _index && rhs._gen = _gen); }
        GenIndex(T index, T gen){ _index = index; _gen = gen; }

        T _index=0, _gen=0; 
        GenIndex & operator=(GenIndex rhs){ 
            _index = rhs._index; _gen = rhs._gen;
            return *this; 
        }
    };
    using entity_t = size_t;
    struct Entity : GenIndex<entity_t> {
        constexpr size_t id() { return _index; }
        Entity(entity_t index=0, entity_t gen=0) : GenIndex<entity_t>(index, gen) {}
    };

    struct Component{ 
        Entity parent_entity; // needed here for fast mulitplexing
        constexpr static size_t max_count = 10;
    };
    // ===============================================================================

    // really simple constant size rolling queue 
    // ===============================================================================
    template<typename T, size_t N>
    struct FixedQueue{
        size_t size() {return _size;}

        void push(T val){
            if(_size < N){
                _first--;
                _arr[_first] = val;
                _size++;
            }
        }
        T pop(){
            T ret = _arr[_first];
            if(_size != 0){
                _size--;
                _first++;
                //_first = (_first+1)%N;
            }
            return ret;
        }

        std::array<T,N> _arr;
        size_t _first=0, _last=0, _size=0;
    };
    // ===============================================================================
    template<typename ...CTs> struct ECS {
        static_assert(std::is_same<Component,typename std::common_type<Component,CTs...>::type>::value);
 
        constexpr static size_t components_count = sizeof...(CTs);
        constexpr static size_t entity_max_count = 2000;


        template<typename T, size_t N> 
        struct ArrayAllocator{
            T  operator [] (size_t i) const { return arr[i]; }
            T& operator [] (size_t i)       { return arr[i]; }
            FixedQueue<size_t, N> free_indices{
                ._arr = make_array_from_sequence(make_sequence<N>()),
                ._first = 1, ._last = N-1, ._size = N-1 
                // index 0 is used here as empty
            };
            std::array<T, N> arr;
            size_t allocate(){ return free_indices.pop(); }
            // freeing two times destroys everything
            void free(size_t index){ free_indices.push(index); }
        };

        

        struct EntityEx{
            entity_t   operator[] (size_t i) const { return components_indices[i]; }
            entity_t & operator[] (size_t i)       { return components_indices[i]; }
            //std::array< GenIndex<entity_t>, sizeof...(CTs)> components_indices;
            std::array< entity_t, sizeof...(CTs)> components_indices;
            entity_t last_gen = 0;
        };


        inline static std::tuple< ArrayAllocator< CTs,CTs::max_count >... > component_arrs;
        inline static ArrayAllocator<EntityEx,entity_max_count> entity_data ;
        inline static entity_t last_free_id = 0;

        template <typename ...Cs>
        static Entity create_entity(){
            entity_t index = entity_data.allocate();
            Entity ret(index, entity_data[index].last_gen);
            (void) std::initializer_list<size_t>{ create_component<Cs>(ret)... };
            return ret;
        }
        template<typename C>
        static size_t create_component(Entity e){ 
            auto& carr = get_component_arr<C>();
            size_t index = carr.allocate(); 
            dmux_mut<C>(e.id()) = index;
            carr[index].parent_entity = e;
            return index;
        }
        static void delete_entity(Entity e){ 
            print(entity_data[e.id()].last_gen , " ", e.gen(), " - ", e.id(), '\n');
            assert(entity_data[e.id()].last_gen == e.gen());
            entity_data[e.id()].last_gen++;
            (void) std::initializer_list<int>{ (delete_component<CTs>(e),0)... };
            entity_data.free(e.id()); 
        }
        template<typename C>
        static void delete_component(Entity e){ 
            if(dmux<C>(e) != 0){
                get_component_arr<C>()[ dmux<C>(e) ].parent_entity._inder = 0;
                get_component_arr<C>().free( dmux<C>(e) );
                dmux_mut<C>(e) = 0;
            }
        }
        template<typename C>
        static C& get_component_mut(Entity e){ 
            //assert(dmux<C>(e) != 0);
            return get_component_arr<C>()[ dmux<C>(e) ];
        }
        template<typename C>
        constexpr static ArrayAllocator<C, C::max_count>& get_component_arr(){
            return std::get< typelist_index_of< C,CTs... >::value >(component_arrs);
        }
        template<typename C>
        constexpr static size_t dmux(Entity e){
            return entity_data[e.id()][typelist_index_of<C, CTs...>::value];
        }
        
        template<typename C>
        constexpr static size_t& dmux_mut(Entity e){
            return entity_data[e.id()][typelist_index_of<C, CTs...>::value];
        }
#if 0
        void printecs(){
            
            for(size_t e=1; e<ecs::entity_max_count; e++) {
                print("entity[", std::setw(1), e,  "] ");
                for(auto i : ecs::entity_data[e].components_indices){
                    if(i == 0){ std::cout << "  "; }
                    else{ std::cout << i << " "; }
                }
                std::cout << '\n';
            }
        }
#endif
    };
}

