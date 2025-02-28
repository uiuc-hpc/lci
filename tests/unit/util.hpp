namespace util
{
void write_buffer(void* buffer, size_t size, const char a)
{
  memset(buffer, a, size);
}

void check_buffer(void* buffer, size_t size, const char a)
{
  for (size_t i = 0; i < size; i++) {
    ASSERT_EQ(((char*)buffer)[i], a);
  }
}

}  // namespace util