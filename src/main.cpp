#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>



template<class T> T min(T a) { return a; }
template<class T, class... Args> T min(T a, Args... args) { return std::min(a, min(args...)); }
template<class T> T max(T a) { return a; }
template<class T, class... Args> T max(T a, Args... args) { return std::max(a, max(args...)); }

template<size_t N> struct VecMembers { std::array<float,N> m; };
template<> struct VecMembers<2> { union { std::array<float,2> m{}; struct { float x, y; }; }; };
template<> struct VecMembers<3> { union { std::array<float,3> m{}; struct { float x, y, z; }; }; };
template<> struct VecMembers<4> { union { std::array<float,4> m{}; struct { float x, y, z, w; }; }; };

template<size_t N>
struct Vec : VecMembers<N> {
	Vec() {}
	template<size_t M, class=std::enable_if_t<N<=M>>
	Vec(const Vec<M>& v) {
		std::copy(v.m.begin(), v.m.begin() + N, this->m.begin());
	}
	template<size_t M, class... Args>
	Vec(const Vec<M>& v, Args... args) {
		std::copy(v.m.begin(), v.m.end(), this->m.begin());
		set<M>(args...);
	}
	template<class... Args>
	Vec(Args... args) { set(args...); }
	template<size_t I> void set() { static_assert(I == N, "wrong argument count"); }
	template<size_t I=0, class... Args>
	void set(float f, Args... args) {
		this->m[I] = f;
		set<I+1>(args...);
	}
	float operator[](size_t i) const { return this->m[i]; }
	float& operator[](size_t i) { return this->m[i]; }
	Vec<N> operator+(const Vec<N>& b) const {
		Vec<N> c;
		for (size_t i = 0; i < N; i++) c[i] = this->m[i] + b[i];
		return c;
	}
	Vec<N> operator-(const Vec<N>& b) const {
		Vec<N> c;
		for (size_t i = 0; i < N; i++) c[i] = this->m[i] - b[i];
		return c;
	}
	float operator*(const Vec<N>& b) const {
		float d = this->m[0] * b[0];
		for (size_t i = 1; i < N; i++) d += this->m[i] * b[i];
		return d;
	}
	Vec<N> operator*(float f) const {
		Vec<N> c;
		for (size_t i = 0; i < N; i++) c[i] = this->m[i] * f;
		return c;
	}
	float length() const {
		return sqrtf(*this * *this);
	}
	Vec<N> normalized() const {
		return *this * (1 / length());
	}
	Vec<N> normalized_fast() const {
		float q = *this * *this;
		float w = q * 0.5;
		uint32_t i = *(uint32_t*) &q;
		i  = 0x5f3759df - (i >> 1);
		q  = *(float*) &i;
		q  *= 1.5 - (w * q * q);
		return *this * q;
	}
};
using Vec2 = Vec<2>;
using Vec3 = Vec<3>;
using Vec4 = Vec<4>;

float cross(const Vec2& a, const Vec2& b) {
	return a.x * b.y - a.y * b.x;
}
Vec3 cross(const Vec3& a, const Vec3& b) {
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}


struct Matrix {
	Matrix operator*(const Matrix& b) {
		Matrix c;
		for (int y = 0; y < 4; y++)
		for (int x = 0; x < 4; x++)
		for (int i = 0; i < 4; i++) c.m[y][x] += m[y][i] * b.m[i][x];
		return c;
	}
	Vec3 operator*(const Vec3& b) {
		Vec3 c;
		for (int y = 0; y < 3; y++)
		for (int x = 0; x < 3; x++) c[y] += m[y][x] * b[x];
		return c;
	}
	Vec4 operator*(const Vec4& b) {
		Vec4 c;
		for (int y = 0; y < 4; y++)
		for (int x = 0; x < 4; x++) c[y] += m[y][x] * b[x];
		return c;
	}
	std::array<std::array<float,4>,4> m = {};
};



struct Color { uint8_t b, g, r, a; };
struct Canvas {
	void init(SDL_Surface* s) {
		width = s->w;
		height = s->h;
		data = (uint32_t*) s->pixels;
		zbuffer.resize(s->w * s->h);
	}
	void clear() {
		memset(data, 0, height * width * 4);
		for (auto& d : zbuffer) d = -9e9;
	}
	void pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, float depth=0) {
		if (x < 0 || x >= width || y < 0 || y >= height) return;
		if (depth > zbuffer[y * width + x]) {
			zbuffer[y * width + x] = depth;
			data[y * width + x] = r | g << 8 | b << 16;
		}
	}

	int			width;
	int			height;
	uint32_t*	data;
	std::vector<float>	zbuffer;
};



Matrix modelview;
Matrix projection;

bool smooth = true;

Vec3 sample(SDL_Surface* s, int u, int v) {
	Uint8* p = (Uint8*) s->pixels + v * s->pitch + u * 3;
	return Vec3 { p[2], p[1], p[0] } * (1 / 255.0);
}

Vec3 smooth_sample(SDL_Surface* s, const Vec2& t) {
	float uf = t.x * s->w;
	float vf = t.y * s->h;
	int u = floor(uf);
	int v = floor(vf);
	uf -= u;
	vf -= v;
	return	(sample(s, u, v) * (1-uf) + sample(s, u+1, v) * uf) * (1-vf) +
			(sample(s, u, v+1) * (1-uf) + sample(s, u+1, v+1) * uf) * vf;
}

Vec3 sample(SDL_Surface* s, const Vec2& t) {
	if (smooth) return smooth_sample(s, t);
	Uint8* p = (Uint8*) s->pixels + (int) (t.y * s->h) * s->pitch + (int) (t.x * s->w) * 3;
	return Vec3 { p[2], p[1], p[0] } * (1 / 255.0);
}



struct Shader {

	SDL_Surface* tex_d;
	SDL_Surface* tex_s;
	SDL_Surface* tex_n;

	Shader() {
		tex_d = IMG_Load("datasets/Ironman/ironman_d.tga");
		tex_s = IMG_Load("datasets/Ironman/ironman_s.tga");
		tex_n = IMG_Load("datasets/Ironman/ironman_n.tga");
	}
	~Shader() {
		SDL_FreeSurface(tex_d);
		SDL_FreeSurface(tex_s);
		SDL_FreeSurface(tex_n);
	}

	bool normal_mapping = true;
	bool diffuse_mapping = true;
	bool specular_mapping = true;
	void toggle_normal_mapping() { normal_mapping = !normal_mapping; }
	void toggle_diffuse_mapping() { diffuse_mapping = !diffuse_mapping; }
	void toggle_specular_mapping() { specular_mapping = !specular_mapping; }

	struct Varying {
		Vec3 pos;
		Vec3 normal;
		Vec2 texcoord;
	};
	Varying varying[3];

	Vec4 vertex(const Vec3& pos, const Vec3& normal, const Vec2& texcoord, Varying& v) {
		v.pos = pos;
		v.normal = modelview * Vec4(normal, 0);
		v.texcoord = texcoord;
		return projection * modelview * Vec4(pos, 1);
	}

	bool fragment(const Vec3& bar, Vec3& color) {

		Vec2 texcoord = {
			Vec3(varying[0].texcoord.x, varying[1].texcoord.x, varying[2].texcoord.x) * bar,
			Vec3(varying[0].texcoord.y, varying[1].texcoord.y, varying[2].texcoord.y) * bar,
		};

		Vec3 normal = {
			Vec3(varying[0].normal.x, varying[1].normal.x, varying[2].normal.x) * bar,
			Vec3(varying[0].normal.y, varying[1].normal.y, varying[2].normal.y) * bar,
			Vec3(varying[0].normal.z, varying[1].normal.z, varying[2].normal.z) * bar,
		};
		normal = normal.normalized_fast();


		if (normal_mapping) {
			Vec3 f1 = varying[1].pos - varying[0].pos;
			Vec3 f2 = varying[2].pos - varying[0].pos;
			Vec2 t1 = varying[1].texcoord - varying[0].texcoord;
			Vec2 t2 = varying[2].texcoord - varying[0].texcoord;
			float coef = 1 / cross(t1, t2);
			Vec3 T = Vec3 {
				f1.x * t2.x + f2.x * -t1.x,
				f1.y * t2.x + f2.y * -t1.x,
				f1.z * t2.x + f2.z * -t1.x,
			} * coef;
			T = T.normalized_fast();
			Vec3 B = cross(normal, T);
			Vec3 n = sample(tex_n, texcoord) * 2 - Vec3(1, 1, 1);
			normal = (T * n.y + B * n.x + normal * n.z).normalized_fast();
		}



		Vec3 light = Vec3{0.5, 1, 2}.normalized();


		float diff = std::max<float>(0, normal * light) * 0.9 + 0.1;
		if (diffuse_mapping) color = sample(tex_d, texcoord) * diff;
		else color = Vec3(0.4, 0.4, 0.6) * diff;


		if (specular_mapping) {
			Vec3 refl = normal * (normal * light * 2) - light;
			//float spec = powf(max(refl.z, 0.0f), 96); // slow
			float spec = max(refl.z, 0.0f);
			spec *= spec; spec *= spec; spec *= spec; spec *= spec; spec *= spec; spec *= spec;
			color = color + sample(tex_s, texcoord) * spec;
		}



//		// toony colors
//		color = color * 5;
//		color.x = (int)color.x / 4.0;
//		color.y = (int)color.y / 4.0;
//		color.z = (int)color.z / 4.0;

		return true;
	}

};


void triangle(Canvas& canvas, Shader& shader, const Vec4& v0, const Vec4& v1, const Vec4& v2) {

	Vec3 oow = { 1 / v0.w, 1 / v1.w, 1 / v2.w };

	auto f0 = Vec2(v0) * oow[0];
	auto f1 = Vec2(v1) * oow[1];
	auto f2 = Vec2(v2) * oow[2];


	Vec2 p0 = f1 - f0;
	Vec2 p1 = f2 - f0;
	if (cross(p0, p1) >= 0) return;

	int y1 = floor(min(f0.y, f1.y, f2.y));
	int y2 = ceil(max(f0.y, f1.y, f2.y));
	int x1 = floor(min(f0.x, f1.x, f2.x));
	int x2 = ceil(max(f0.x, f1.x, f2.x));

	if (x2 < 0 || x1 > canvas.width
	||	y2 < 0 || y1 > canvas.height) return;

	x1 = max(x1, 0);
	x2 = min(x2, canvas.width);
	y1 = max(y1, 0);
	y2 = min(y2, canvas.height);

	float d00 = p0 * p0;
	float d01 = p0 * p1;
	float d11 = p1 * p1;

	for (int y = y1; y < y2; y++)
	for (int x = x1; x < x2; x++) {

		Vec2 p2 = Vec2(x, y) - f0;
		float d20 = p2 * p0;
		float d21 = p2 * p1;
		float qqq = d00 * d11 - d01 * d01;
		if (std::abs(qqq) < 0.01) continue;
		qqq = 1 / qqq;

		Vec3 bar;
		bar.y = (d11 * d20 - d01 * d21) * qqq;
		bar.z = (d00 * d21 - d01 * d20) * qqq;
		bar.x = 1 - bar.z - bar.y;

		if (bar.x < 0 || bar.y < 0 || bar.z < 0) continue;
//		if (bar.x < 0.02 || bar.y < 0.02 || bar.z < 0.02) continue;


		bar = Vec3 {
			bar.x * oow.x,
			bar.y * oow.y,
			bar.z * oow.z,
		} * (1 / (bar * oow));


		Vec3 color;
		if (shader.fragment(bar, color)) {
			float z = Vec3(v0.z, v1.z, v2.z) * bar;
			float w = Vec3(v0.w, v1.w, v2.w) * bar;
			canvas.pixel(x, y,
				(uint8_t) max(0, min(255, int(color.z * 255))),
				(uint8_t) max(0, min(255, int(color.y * 255))),
				(uint8_t) max(0, min(255, int(color.x * 255))),
				z / w);
		}

	}
}



class Model {
public:
	bool load(const char* name) {
		FILE* f = fopen(name, "r");
		if (!f) return false;
		positions.clear();
		normals.clear();
		texcoords.clear();
		faces.clear();
		char line[256];
		while (char* p = fgets(line, sizeof(line), f)) {
			auto next = [&p](const char* delim=" \t") -> char* {
				if (p) p += strspn(p, delim);
				return strsep(&p, delim);
			};
			char* cmd = next();
			if (cmd[0] == '#') continue;
			if (strcmp(cmd, "v") == 0) {
				Vec3 v;
				v.x = atof(next());
				v.y = atof(next());
				v.z = atof(next());
				positions.emplace_back(v);
			}
			if (strcmp(cmd, "vn") == 0) {
				Vec3 v;
				v.x = atof(next());
				v.y = atof(next());
				v.z = atof(next());
				normals.emplace_back(v);
			}
			if (strcmp(cmd, "vt") == 0) {
				Vec2 t;
				t.x = atof(next());
				t.y = 1 - atof(next());
				t.y = fmodf(t.y, 1);
				texcoords.emplace_back(t);
			}
			if (strcmp(cmd, "f") == 0) {
				faces.emplace_back();
				auto& f = faces.back();
				while (char* q = next()) {
					Vertex v;
					v.p = atoi(strsep(&q, "/")) - 1;
					if (q) v.t = atoi(strsep(&q, "/")) - 1;
					if (q) v.n = atoi(q) - 1;
					f.emplace_back(v);
				}
			}
		}
		fclose(f);
		return true;
	}

	void draw(Canvas& canvas, Shader& shader) {
		for (auto& f : faces) {
			auto& v0 = f[0];
			for (size_t i = 2; i < f.size(); i++) {
				auto& v1 = f[i - 1];
				auto& v2 = f[i];
				triangle(canvas, shader,
					shader.vertex(positions[v0.p], normals[v0.n], texcoords[v0.t], shader.varying[0]),
					shader.vertex(positions[v1.p], normals[v1.n], texcoords[v1.t], shader.varying[1]),
					shader.vertex(positions[v2.p], normals[v2.n], texcoords[v2.t], shader.varying[2]));
			}
		}
	}

private:
	struct Vertex { int p, n, t; };
	std::vector<Vec3> positions;
	std::vector<Vec3> normals;
	std::vector<Vec2> texcoords;

	std::vector<std::vector<Vertex>> faces;
};




int main(int argc, char** argv) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("render",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);

	Model mod;
	mod.load("datasets/Ironman/Ironman.obj");
	Canvas canvas;
	SDL_Surface* surface = SDL_GetWindowSurface(window);
	canvas.init(surface);
	Shader shader;

	float ang = -1.4;

	bool running = true;
	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_QUIT:
				running = false;
				break;

			case SDL_KEYDOWN:
				if (e.key.keysym.scancode == SDL_SCANCODE_N) shader.toggle_normal_mapping();
				if (e.key.keysym.scancode == SDL_SCANCODE_D) shader.toggle_diffuse_mapping();
				if (e.key.keysym.scancode == SDL_SCANCODE_S) shader.toggle_specular_mapping();
				if (e.key.keysym.scancode == SDL_SCANCODE_F) smooth = !smooth;
				break;

			case SDL_WINDOWEVENT:
				if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
					surface = SDL_GetWindowSurface(window);
					canvas.init(surface);
				}
				break;
			}
		}

		float w = canvas.width / 2.0;
		float h = canvas.height / 2.0;
		projection = Matrix {
			 w,   0,   0,    w,
			 0,  -w,   0,    h,
			 0,   0, 127.5,127.5,
			 0,   0,   0,    1,
		} * Matrix {
			 1,   0,    0,   0,
			 0,   1,    0,   0,
			 0,   0,    1,   0,
			 0,   0,   -0.3, 1,
		};

		ang += 0.01;
		float s = sin(ang);
		float c = cos(ang);
		modelview = Matrix {
			 1,   0,   0,   0,
			 0,   1,   0,  -1.8,
			 0,   0,   1,  -8,
			 0,   0,   0,   1,
		} * Matrix {
			 c,   0,   s,   0,
			 0,   1,   0,   0,
			-s,   0,   c,   0,
			 0,   0,   0,   1,
		};

		SDL_LockSurface(surface);
		canvas.clear();
		mod.draw(canvas, shader);
		SDL_UnlockSurface(surface);
		SDL_UpdateWindowSurface(window);
	}

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
