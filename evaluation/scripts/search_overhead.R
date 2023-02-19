file_name <- "pos1_16384_lazy_d10"

data <- read.table(paste0(file_name, ".txt"), header = TRUE)

# Aggregate data by ply and num_thr, and calculate the average time
data_agg <- aggregate(nodes ~ ply + num_thr, data, mean)

# Normalize nps by dividing by nps at num_thr = 1
data_norm <- merge(data_agg, subset(data_agg, num_thr == 1), by = "ply", suffixes = c("", "_1"))
data_norm$nodes_norm <- data_norm$nodes / data_norm$nodes_1

data_trimmed <- subset(data_norm, ply >= 5)
max_threads = max(data_norm$num_thr)

library(ggplot2)

pdf(paste0(file_name, "_so.pdf"))
ggplot(data_trimmed, aes(x=num_thr, y=nodes_norm, group=ply)) +
  geom_line(aes(color=ply)) +
  scale_color_viridis_c() +
  scale_x_continuous(breaks = c(1, max_threads / 4, max_threads / 2, max_threads * 3 / 4, max_threads)) +
  labs(title = "Search Overhead vs Number of Threads",
       x = "Number of Threads",
       y = "Search Overhead",
       color = "Ply")

dev.off()
