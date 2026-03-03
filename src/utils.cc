static std::string read_file(const char* file_path) {
	std::ifstream file{file_path, std::ios::binary};
	if (file.fail()) {
		Log("Failed to open file: %s\n", file_path);
		return {};
	}
	std::stringstream file_stream;
	file_stream << file.rdbuf();
	return file_stream.str();
}
