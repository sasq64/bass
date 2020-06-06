#pragma once

#include <cmath>
#include <initializer_list>
#include <string.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace utils {

template <class T, int SIZE> struct vbase {
    union {
        T data[SIZE];
        struct {
            T x, y, z, w;
        };
    };
};

// Pointless ?
template <class T> struct vbase<T, 1> {
    union {
        T data[1];
    };
};

template <class T> struct vbase<T, 2> {
    vbase() { memset(data, 0, 2 * sizeof(T)); }
    vbase(const T& x, const T& y) : data{x, y} {}
    union {
        T data[2];
        struct {
            T x, y;
        };
    };

    vbase operator*(const vbase& v) const { return vbase(v.x + x, v.y + y); }

    template <typename R> using rr = std::remove_reference<R>;

    operator std::tuple<T, T>() const { return std::tuple<T, T>(x, y); }

    std::tuple<T, T> to_tuple() const { return std::tuple<T, T>(x, y); }
};

template <class T> struct vbase<T, 3> {
    vbase() { memset(data, 0, 3 * sizeof(T)); }
    vbase(const T& x, const T& y, const T& z) : data{x, y, z} {}
    template <typename X, typename Y, typename Z>
    operator std::tuple<X, Y, Z>() const {
        return std::tuple<X, Y, Z>(x, y, z);
    }
    union {
        T data[3];
        struct {
            T x, y, z;
        };
    };
};

template <class T> struct vbase<T, 4> {
    vbase() { memset(data, 0, 4 * sizeof(T)); }
    vbase(const T& x, const T& y, const T& z) : data{x, y, z} {}
    template <typename X, typename Y, typename Z, typename W>
    operator std::tuple<X, Y, Z, W>() const {
        return std::tuple<X, Y, Z, W>(x, y, z, w);
    }
    union {
        T data[4];
        struct {
            T x, y, z, w;
        };
    };
};

template <typename C, typename R>
using is_compound =
    typename std::enable_if<std::is_compound<C>::value, R>::type;

template <typename C, typename R>
using is_arithmetic =
    typename std::enable_if<std::is_arithmetic<C>::value, R>::type;

template <typename C, typename R>
using has_index =
    typename std::conditional<false, decltype(std::declval<C>()[0]), R>::type;


template <typename T> T from_string(std::string const& s)
{
    if constexpr (std::is_floating_point<T>::value)
        return stod(s);
    else
        return stoll(s);
}

template <class T, int SIZE> struct vec : public vbase<T, SIZE> {

    vec() : vbase<T, SIZE>() {}

    vec(const T& x) : vbase<T, SIZE>(x) {}
    vec(const T& x, const T& y) : vbase<T, SIZE>(x, y) {}
    vec(const T& x, const T& y, const T& z) : vbase<T, SIZE>(x, y, z) {}

    explicit vec(const std::string& t) {
        int i = 0;
        T* ptr = &vbase<T, SIZE>::data[0];
        int start = 0;
        auto len = t.length();
        while(i < len) {
            if(t[i] == ';' || t[i] == ':') {
                *ptr++ = from_string<T>(t.substr(start, i));
                start = i + 1;
            }
            i++;
        }
        if(start < len)
            *ptr++ = from_string<T>(t.substr(start));
    }

    vec(const std::pair<T, T>& pair)
        : vbase<T, SIZE>(pair.first, pair.second) {}

    T* get() { return &vbase<T, SIZE>::data[0]; }

    vec(const std::initializer_list<T>& il) {
        T* ptr = &vbase<T, SIZE>::data[0];
        for(const T& t : il)
            *ptr++ = t;
    }

    bool operator==(const vec& v) const {
        for(int i = 0; i < SIZE; i++)
            if(v[i] != vbase<T, SIZE>::data[i])
                return false;
        return true;
    }

    template <typename VEC> has_index<VEC, vec> copy(const VEC& v) {}

    template <typename VEC> has_index<VEC, vec> operator+(const VEC& v) const {
        vec r;
        for(int i = 0; i < SIZE; i++)
            r[i] = vbase<T, SIZE>::data[i] + v[i];
        return r;
    }

    template <typename I> is_arithmetic<I, vec> operator+(const I& t) const {
        vec r;
        for(int i = 0; i < SIZE; i++)
            r[i] = vbase<T, SIZE>::data[i] + t;
        return r;
    }

    template <typename VEC> has_index<VEC, vec> operator+=(const VEC& v) {
        for(int i = 0; i < SIZE; i++)
            vbase<T, SIZE>::data[i] += v[i];
        return *this;
    }

    template <typename S> vec operator+=(const std::initializer_list<S>& il) {
        T* ptr = &vbase<T, SIZE>::data[0];
        for(const S& t : il)
            *ptr++ += t;
        return *this;
    }

    template <typename VEC> has_index<VEC, vec> operator*(const VEC& v) const {
        vec r;
        for(int i = 0; i < SIZE; i++)
            r[i] = vbase<T, SIZE>::data[i] * v[i];
        return r;
    }

    template <typename I> is_arithmetic<I, vec> operator*(const I& t) const {
        vec r;
        for(int i = 0; i < SIZE; i++)
            r[i] = vbase<T, SIZE>::data[i] * t;
        return r;
    }

    template <typename VEC> has_index<VEC, vec> operator-(const VEC& v) const {
        vec r;
        for(int i = 0; i < SIZE; i++)
            r[i] = vbase<T, SIZE>::data[i] - v[i];
        return r;
    }

    template <typename I> is_arithmetic<I, vec> operator-(const I& t) const {
        vec r;
        for(int i = 0; i < SIZE; i++)
            r[i] = vbase<T, SIZE>::data[i] - t;
        return r;
    }

    template <typename VEC> has_index<VEC, vec> operator/(const VEC& v) const {
        vec r;
        for(int i = 0; i < SIZE; i++)
            r[i] = vbase<T, SIZE>::data[i] / v[i];
        return r;
    }

    template <typename I> is_arithmetic<I, vec> operator/(const I& t) const {
        vec r;
        for(int i = 0; i < SIZE; i++)
            r[i] = vbase<T, SIZE>::data[i] / t;
        return r;
    }

    const T& operator[](const int& i) const { return vbase<T, SIZE>::data[i]; }

    T& operator[](const int& i) { return vbase<T, SIZE>::data[i]; }

    T dist2() {
        T rc;
        for(int i = 0; i < SIZE; i++)
            rc += vbase<T, SIZE>::data[i] * vbase<T, SIZE>::data[i];
        return rc;
    }

    T dist() {
        T rc = 0;
        for(int i = 0; i < SIZE; i++)
            rc += vbase<T, SIZE>::data[i] * vbase<T, SIZE>::data[i];
        return sqrt(rc);
    }

    vec normalize() { return *this / dist(); }

    template <typename VEC> has_index<VEC, T> dot(const VEC& v) const {
        T r = 0;
        for(int i = 0; i < SIZE; i++)
            r += (vbase<T, SIZE>::data[i] * v[i]);
        return r;
    }

    std::string toString() {
        char temp[256] = "[";
        char* ptr = temp + 1;

        for(int i = 0; i < SIZE; i++) {
            printf(ptr, "%f ", vbase<T, SIZE>::data[i]);
            ptr += (strlen(ptr));
        }
        ptr[-1] = ']';
        return std::string(ptr);
    }
};

typedef vec<double, 2> vec2;
typedef vec<double, 3> vec3;
typedef vec<double, 4> vec4;

typedef vec<float, 2> vec2f;
typedef vec<float, 3> vec3f;
typedef vec<float, 4> vec4f;

typedef vec<int, 2> vec2i;
typedef vec<int, 3> vec3i;
typedef vec<int, 4> vec4i;

template <class T, int SIZE> vec<T, SIZE> sin(const vec<T, SIZE>& v) {
    vec<T, SIZE> r;
    for(int i = 0; i < SIZE; i++)
        r[i] = sinf(v[i]);
    return r;
}

/*
template <class T, int SIZE> struct mat {

    mat() {
        for(int i=0; i<SIZE*SIZE; i++)
            data[i/SIZE][i%SIZE] = i%(SIZE+1) ? 0.0 : 1.0;
    }

    vec<T, SIZE> operator*(const vec<T, SIZE> &v) const {
        vec<T, SIZE> r;
        for(int i=0; i<SIZE; i++)
            r[i] = data[i].dot(v);
        return r;
    }

    std::string to_string() {
        std::string s;
        for(int i=0; i<SIZE; i++) {
            for(int j=0; j<SIZE; j++) {
                s += std::to_string(data[i][j]);
                s += " ";
            }
            s += "\n";
        }

            return s;
    }

    vec<T, SIZE> data[SIZE];
};
*/

template <class T, class... S>
vec<T, sizeof...(S) + 1> make_vec(const T& a0, const S&... args) {
    return vec<T, sizeof...(S) + 1>({a0, args...});
}

} // namespace utils
