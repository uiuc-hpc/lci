use indicatif::{MultiProgress, ProgressBar, ProgressStyle};
use regex::Regex;
use std::env;
use std::fs::File;
use std::io::{self, BufRead, BufReader};
use std::collections::{HashMap, HashSet};
use csv::Writer;
use std::sync::{Arc, Mutex};
use std::thread;

#[derive(Debug, Clone)]
struct MessageEvent {
    time: f64,
    local_rank: String,
    remote_rank: String,
    user_type: String,
    size: String,
}

#[derive(Debug, Clone)]
struct MessageStats {
    count: usize,
    min_latency: f64,
    max_latency: f64,
    avg_latency: f64,
    std_latency: f64,
}

fn calculate_stats(latencies: &[f64]) -> MessageStats {
    if latencies.is_empty() {
        return MessageStats {
            count: 0,
            min_latency: 0.0,
            max_latency: 0.0,
            avg_latency: 0.0,
            std_latency: 0.0,
        };
    }

    let count = latencies.len();
    let min_latency = latencies.iter().copied().reduce(f64::min).unwrap();
    let max_latency = latencies.iter().copied().reduce(f64::max).unwrap();
    
    let avg_latency = latencies.iter().sum::<f64>() / count as f64;
    
    let variance = latencies.iter()
        .map(|&x| (x - avg_latency).powi(2))
        .sum::<f64>() / count as f64;
    let std_latency = variance.sqrt();

    MessageStats {
        count,
        min_latency,
        max_latency,
        avg_latency,
        std_latency,
    }
}

fn parse_lines(
    lines: Vec<String>, 
    re: &Regex, 
    send_bins: &Arc<Mutex<HashMap<(String, String), Vec<MessageEvent>>>>,
    recv_bins: &Arc<Mutex<HashMap<(String, String), Vec<MessageEvent>>>>,
    progress_bar: &ProgressBar
) {
    for line in lines {
        progress_bar.inc(1);
        
        if let Some(caps) = re.captures(&line) {
            let time: f64 = caps[1].parse().unwrap();
            let local_rank = caps[2].to_string();
            let thread_id = caps[3].to_string();
            let remote_rank = caps[6].to_string();
            let user_type = caps[5].to_string();
            let size = caps[7].to_string();
            let operation = &caps[4];

            let key = (local_rank.clone(), remote_rank.clone());

            let event = MessageEvent {
                time,
                local_rank,
                remote_rank,
                user_type,
                size,
            };

            match operation {
                "send" => {
                    let mut send_bins_lock = send_bins.lock().unwrap();
                    send_bins_lock.entry(key).or_default().push(event);
                },
                "recv" => {
                    let mut recv_bins_lock = recv_bins.lock().unwrap();
                    recv_bins_lock.entry(key).or_default().push(event);
                },
                _ => eprintln!("Unexpected operation: {}", operation),
            }
        }
    }
}

fn match_messages(
    send_bin: Vec<MessageEvent>, 
    recv_bin: Vec<MessageEvent>,
    matched_messages: &Arc<Mutex<Vec<(MessageEvent, MessageEvent, f64)>>>,
    rank_latencies: &Arc<Mutex<HashMap<(String, String), Vec<f64>>>>
) {
    for send in send_bin {
        for recv in &recv_bin {
            let latency = recv.time - send.time;
            
            let mut matched_messages_lock = matched_messages.lock().unwrap();
            matched_messages_lock.push((send.clone(), recv.clone(), latency));
            
            let rank_pair = (send.local_rank.clone(), send.remote_rank.clone());
            
            let mut rank_latencies_lock = rank_latencies.lock().unwrap();
            rank_latencies_lock.entry(rank_pair).or_default().push(latency);
        }
    }
}

fn main() -> io::Result<()> {
    let args: Vec<String> = env::args().collect();
    if args.len() != 3 {
        eprintln!("Usage: {} <file_path> <num_threads>", args[0]);
        std::process::exit(1);
    }
    let file_path = &args[1];
    let num_threads: usize = args[2].parse().unwrap_or(4);

    let file = File::open(file_path)?;
    let reader = BufReader::new(&file);
    
    let re = Regex::new(r"(?x)
        (\d+\.\d+)                     
        \s+(\d+)/(\d+):                
        \s+(send|recv)                 
        \s+(\d+)\s+(\d+)\s+(\d+)       
    ").unwrap();

    let total_lines: Vec<String> = reader.lines().map(|l| l.unwrap()).collect();
    let chunk_size = total_lines.len() / num_threads;

    let multi_progress = MultiProgress::new();
    let parsing_progress = multi_progress.add(ProgressBar::new(total_lines.len() as u64));
    parsing_progress.set_style(
        ProgressStyle::with_template("{pos}/{len} [{wide_bar:.cyan/blue}] Parsing")
            .unwrap()
            .progress_chars("#>-")
    );

    let send_bins = Arc::new(Mutex::new(HashMap::new()));
    let recv_bins = Arc::new(Mutex::new(HashMap::new()));

    let mut parse_handles = vec![];

    for chunk in total_lines.chunks(chunk_size) {
        let chunk_lines = chunk.to_vec();
        let re_clone = re.clone();
        let send_bins_arc = Arc::clone(&send_bins);
        let recv_bins_arc: Arc<Mutex<HashMap<(String, String), Vec<MessageEvent>>>> = Arc::clone(&recv_bins);
        let progress_clone = parsing_progress.clone();

        let handle = thread::spawn(move || {
            parse_lines(chunk_lines, &re_clone, &send_bins_arc, &recv_bins_arc, &progress_clone);
        });
        parse_handles.push(handle);
    }

    for handle in parse_handles {
        handle.join().unwrap();
    }
    parsing_progress.finish_with_message("Parsing complete");

    let matched_messages = Arc::new(Mutex::new(Vec::new()));
    let rank_latencies = Arc::new(Mutex::new(HashMap::new()));

    let mut match_handles = vec![];
    let send_bins_lock = send_bins.lock().unwrap();
    let recv_bins_lock = recv_bins.lock().unwrap();

    for ((local_rank, remote_rank), send_bin) in send_bins_lock.iter() {
        if let Some(recv_bin) = recv_bins_lock.get(&(local_rank.clone(), remote_rank.clone())) {
            let matched_messages_arc = Arc::clone(&matched_messages);
            let rank_latencies_arc = Arc::clone(&rank_latencies);
            let send_bin_clone = send_bin.clone();
            let recv_bin_clone = recv_bin.clone();

            let handle = thread::spawn(move || {
                match_messages(send_bin_clone, recv_bin_clone, &matched_messages_arc, &rank_latencies_arc);
            });
            match_handles.push(handle);
        }
    }

    for handle in match_handles {
        handle.join().unwrap();
    }

    let mut messages = matched_messages.lock().unwrap().clone();
    let rank_latencies_map = rank_latencies.lock().unwrap().clone();

    let mut wtr = Writer::from_path("messages.csv")?;
    wtr.write_record(&["Local Rank", "Remote Rank", "User Type", "Size", "Latency"])?;
    
    for (send, _, latency) in &messages {
        wtr.write_record(&[
            &send.local_rank,
            &send.remote_rank,
            &send.user_type,
            &send.size,
            &format!("{:.6}", latency),
        ])?;
    }
    wtr.flush()?;

    println!("\nMessage Latency Statistics by Rank Pair:");
    
    let statistics_progress = multi_progress.add(ProgressBar::new(rank_latencies_map.len() as u64));
    statistics_progress.set_style(
        ProgressStyle::with_template("{pos}/{len} [{wide_bar:.green/blue}] Calculating Statistics")
            .unwrap()
            .progress_chars("#>-")
    );

    let mut stat_handles = vec![];
    let shared_stats = Arc::new(Mutex::new(Vec::new()));

    for ((local_rank, remote_rank), latencies) in rank_latencies_map {
        let shared_stats_arc = Arc::clone(&shared_stats);
        let statistics_progress_clone = statistics_progress.clone();

        let handle = thread::spawn(move || {
            let stats = calculate_stats(&latencies);
            shared_stats_arc.lock().unwrap().push((local_rank, remote_rank, stats));
            statistics_progress_clone.inc(1);
        });
        stat_handles.push(handle);
    }

    for handle in stat_handles {
        handle.join().unwrap();
    }
    statistics_progress.finish_with_message("Statistics complete");

    let final_stats = shared_stats.lock().unwrap().clone();
    for (local_rank, remote_rank, stats) in final_stats {
        println!(
            "Rank Pair ({}, {}): Count: {}, Min: {:.6}, Max: {:.6}, Avg: {:.6}, Std: {:.6}",
            local_rank, 
            remote_rank, 
            stats.count,
            stats.min_latency,
            stats.max_latency,
            stats.avg_latency,
            stats.std_latency
        );
    }

    println!("\nTotal messages processed: {}", messages.len());
    
    Ok(())
}