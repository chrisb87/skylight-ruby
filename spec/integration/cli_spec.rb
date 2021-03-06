require 'spec_helper'
require 'open3'

describe "CLI integration", :http do

  it "works" do
    server.mock "/me" do |env|
      env['HTTP_X_EMAIL'].should == "test@example.com"
      env['HTTP_X_PASSWORD'].should == "testpass"

      { me: { authentication_token: "testtoken" }}
    end

    server.mock "/apps", :post do |env|
      env['HTTP_AUTHORIZATION'].should == "testtoken"
      env['rack.input'].should == { 'app' => { 'name' => 'Dummy' }}

      # This would have more information really, but the CLI doesn't care
      { app: { id: 'appid', token: 'apptoken' }}
    end

    with_standalone do
      output = `bundle install`
      puts output if ENV['DEBUG']

      Open3.popen3("bundle exec skylight setup") do |stdin, stdout, stderr|
        begin
          get_prompt(stdout).should =~ /Email:\s*$/
          fill_prompt(stdin, "test@example.com")

          get_prompt(stdout).should =~ /Password:\s*$/
          fill_prompt(stdin, "testpass", false)

          read(stdout).should include("Congratulations. Your application is on Skylight!")

          YAML.load_file("../.skylight").should == {"token"=>"testtoken"}

          YAML.load_file("config/skylight.yml").should == {"application"=>"appid", "authentication"=>"apptoken"}
        rescue
          # Provide some potential debugging information
          puts stderr.read if ENV['DEBUG']
          raise
        end
      end
    end
  end

  def get_prompt(io, limit=100)
    prompt = io.readpartial(limit)
    print prompt if ENV['DEBUG']
    prompt
  end

  def fill_prompt(io, str, echo=ENV['DEBUG'])
    io.puts str
    puts str if echo
  end

  def read(io)
    result = io.read
    puts result if ENV['DEBUG']
    result
  end

end