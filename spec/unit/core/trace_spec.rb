require 'spec_helper'

module Skylight
  describe 'Trace', :agent do

    before :each do
      clock.now = 100_000_000
    end

    let! :trace do
      Skylight::Messages::Trace::Builder.new instrumenter, 'Zomg', clock.nanos, 'app.rack.request'
    end

    it 'serializes without crashing' do
      trace.traced

      native = trace.instance_variable_get(:@native_builder)
      native.native_serialize
    end
  end
end
