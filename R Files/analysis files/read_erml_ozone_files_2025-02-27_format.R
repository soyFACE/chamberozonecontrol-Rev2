rm(list = ls())

eolist <- list.files("Z:/ERML chambers ozone data/primary_data/ozone_controller/",
                     pattern = ".csv",
                     full.names = TRUE
)

eoseed <- read.csv(eolist[1], sep = ",",
                   header = FALSE,
                   stringsAsFactors = FALSE,
                   colClasses = "character",
                   col.names = c('datetime','state','setpoint','process_value','error','vout','dfrout','pcomponent','icomponent','dcomponent','ozonestate','ballast_manual','ballast_auto','bulb_manual','bulb_auto','process_value_alternate','door_open','ozonator_temp','ozonator_light_intensity','kp','ki','kd','cycletime'),
                   skip = 0
)



eoseed[1:100000,] <- NA
eoseed$file_source <- NA
eoseed$records <- NA

## Check each file has only 1 date.

running_total <- 0
my_counter <- 0
#i <- 1
for (f in eolist) {
  #  i
  tf <- read.csv(f,
                 header = FALSE,
                 sep = ",",
                 stringsAsFactors = FALSE,
                 colClasses = c('character'),
                 col.names = c('datetime','state','setpoint','process_value','error','vout','dfrout','pcomponent','icomponent','dcomponent','ozonestate','ballast_manual','ballast_auto','bulb_manual','bulb_auto','process_value_alternate','door_open','ozonator_temp','ozonator_light_intensity','kp','ki','kd','cycletime'),
                 skip = 0
  )
  tf$file_source <- f
  records <- nrow(tf)
  tf$records <- records
  #  View(tf)
  eoseed <- rbind(eoseed,tf)
  #  View(eoseed)
  #eoseed[(my_counter+1):(my_counter+records),] <- tf
  my_counter <- my_counter+records
  print(paste(f, records))
  running_total = running_total+records
  #  i <- i+1
  #  f <- eolist[i]
}

eo <- eoseed

rm(eoseed)

eo <- eo[!is.na(eo$datetime),]

eo <- eo[!eo$setpoint != "Setpoint:150.00",]


eo$datetime <- substr(eo$datetime,10,28)
eo$state <- as.numeric(substr(eo$state,7,8))
eo$setpoint <- as.numeric(substr(eo$setpoint,10,16))
eo$process_value <- as.numeric(substr(eo$process_value,15,20))
eo$error <- as.numeric(substr(eo$error,7,11))
#eo$error_calculated <- as.numeric(eo$setpoint) - as.numeric(eo$process_value)
#hist(eo$error_calculated - as.numeric(eo$error))
eo$vout <- as.numeric(substr(eo$vout,6,11))
eo$dfrout <- as.numeric(substr(eo$dfrout,8,12))
eo$pcomponent <- as.numeric(substr(eo$pcomponent,12,18))
eo$icomponent <- as.numeric(substr(eo$icomponent,12,18))
eo$dcomponent <- as.numeric(substr(eo$dcomponent,12,18))
eo$ozonestate <- as.logical(as.numeric(substr(eo$ozonestate,10,11)))
eo$ballast_manual <- as.logical(as.numeric(substr(eo$ballast_manual,16,17)))
eo$ballast_auto <- as.logical(as.numeric(substr(eo$ballast_auto,14,15)))
eo$bulb_manual <- as.logical(as.numeric(substr(eo$bulb_manual,13,14)))
eo$bulb_auto <- as.logical(as.numeric(substr(eo$bulb_auto,11,12)))
eo$door_open <- as.logical(as.numeric(substr(eo$door_open,11,12)))
eo$ozonator_temp <- as.numeric(substr(eo$ozonator_temp,15,18))
eo$ozonator_light_intensity <- as.numeric(substr(eo$ozonator_light_intensity,26,29))
eo$process_value_alternate <- as.numeric(substr(eo$process_value_alternate,25,30))
eo$kp <- as.numeric(substr(eo$kp,4,10))
eo$ki <- as.numeric(substr(eo$ki,4,10))
eo$kd <- as.numeric(substr(eo$kd,4,10))
eo$cycletime <- as.numeric(substr(eo$cycletime,12,16))

row.names(eo) <- NULL


eo$datetime <- as.POSIXct(eo$datetime, format = "%Y-%m-%d %H:%M:%S", tz = "GMT")
eo <- eo[order(eo$datetime),]

eooon <- eo[eo$ozonestate == TRUE,]

first_date <- min(eo$datetime)
last_date <- max(eo$datetime)

main_string <- paste("Chamber ozone levels\nfrom",first_date, "to", last_date)


plot(eo$datetime, eo$process_value, type = 'l'
     #,xlim = c(0,200),
     ,ylim = c(0,200)
     ,ylab = "ozone (ppb)"
     ,xlab = "Date-time"
     ,main = main_string)
#lines(eo$datetime, eo$dfrout/100, col = "blue")

main_string <- paste("Histogram of ozone values and stats while operating\nfrom",first_date, "to", last_date)

hist(eooon$process_value, breaks = seq(-0.5,225.5,1), main = main_string)
hist_mean <- format(trunc(mean(eooon$process_value)),nsmall = 2)
hist_sd <- format(trunc(sd(eooon$process_value)),nsmall = 2)
hist_min <- format(trunc(min(eooon$process_value)),nsmall = 2)
hist_max <- format(trunc(max(eooon$process_value)),nsmall = 2)

stat_text <- paste(hist_mean,hist_sd,hist_min,hist_max, sep = "\n")

text(25,40000,labels = "mean:\nsd:\nmin:\nmax:", adj = c(1,.5))
text(28,40000,labels = stat_text,adj = c(0,.5))


start_date <- as.POSIXct(paste(Sys.Date()-3, "09:50:00"), tz = "GMT")
end_date <- as.POSIXct(paste(Sys.Date(), "16:10:00"), tz = "GMT")
eosub <- eo[eo$datetime >= start_date,]
eosub <- eosub[eosub$datetime <= end_date,]

main_string <- paste("Three day ozone detail\nfrom",start_date, "to", end_date)

plot(eosub$datetime, eosub$process_value, type = 'l'
     #,xlim = c(0,200),
     ,ylim = c(0,250)
     ,ylab = "ozone (ppb)"
     ,xlab = "Date-time"
     ,main = main_string)
abline(h = 150)
abline(h = 153, col = 'grey')
abline(h = 147, col = 'grey')

main_string <- paste("Three Day CO2 detail\nfrom",start_date, "to", end_date)

plot(eosub$datetime, eosub$process_value_alternate, type = 'l'
     #,xlim = c(0,200),
     ,ylim = c(0,600)
     ,ylab = "ozone (ppb)"
     ,xlab = "Date-time"
     ,main = main_string)

main_string <- paste("Performance three day detail\nfrom",start_date, "to", end_date)

plot(eosub$datetime, eosub$process_value, type = 'l'
     #,xlim = c(0,200),
     ,ylim = c(0,250)
     ,ylab = "ozone (ppb)"
     ,xlab = "Date-time"
     ,main = main_string)
abline(h = 150)
abline(h = 153, col = 'grey')
abline(h = 147, col = 'grey')
abline(h = 0)
lines(eosub$datetime, eosub$dfrout/100, col = "blue")
lines(eosub$datetime, eosub$pcomponent*5, col = 'green')
lines(eosub$datetime, eosub$icomponent*5, col = 'yellow')
lines(eosub$datetime, eosub$dcomponent*5+150, col = 'red')



