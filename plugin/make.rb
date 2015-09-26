require 'fileutils'

# Not sure if it is possible to correctly escape %_PDB% in CMAKE.
# See:
#
#    http://stackoverflow.com/questions/30662342/specifying-path-to-pdb-symbol-file-in-cmake
#
# So let's patch it instead.
def patch_pdbaltpath(f)
	s = IO.read(f)
	s = s.gsub('%%_PDB%%', '%_PDB%')
	File.open(f, 'w') {|io| io.write(s)}
end

FileUtils.mkdir_p("build")
Dir.chdir("build") do
	system %q{cmake -G "Visual Studio 11 2012 Win64" ..} or raise "cmake: #{$?}"
    patch_pdbaltpath("city_plugin.vcxproj")
    system %Q{"#{ENV["VS120COMNTOOLS"]}/../../VC/vcvarsall.bat" x86_amd64 && MSBuild.exe city_plugin.sln /t:Build /p:Configuration=Debug /p:Platform=x64} or raise "msbuild: #{$?}"
end