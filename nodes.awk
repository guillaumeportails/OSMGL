#
# Usage :
#    gawk -f nodes.awk toto.osm > toto
#    gnuplot
#      plot 'toto' index 0, '' index 1 with lines

/<node id=/ {
  id=substr($2,5)+0
  lat=substr($3,6)+0.0
  lon=substr($4,6)+0.0
  printf "%10.7f %10.7f   %10d\n", lat, lon, id
  tlat[id]=lat
  tlon[id]=lon
}

/<way id=/ {
  inway=1
  printf "\n\n"
}

/<nd ref=/ {
  if (inway) {
    id=substr($2,6)+0
    printf "%10f %10f\n", tlat[id], tlon[id]
  }
}

/<\/way>/ {
  inway=0
}

