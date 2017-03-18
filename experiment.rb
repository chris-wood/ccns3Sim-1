times = [1000, 2000]
num_contents = [10, 25, 50, 100, 1000]
cache_fractions = [0.25, 0.5, 1.0, 2.0, 5.0]

times.each{|time|
    num_contents.each{|n|
        cache_fractions.each{|cf|
            data_file = "#{time}_#{n}_#{cf}"
            run_result = `../../waf --run="ccnx-monitor --time=#{time} --numberContents=#{n} --cacheSize=#{(n * cf).to_i}" 2> #{data_file}.txt`
            plot_result = `python dist_plotter.py #{data_file}.txt #{data_file}`
        }
    }
}
