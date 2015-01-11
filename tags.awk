# Analyse de l'emploi des tags dans un OSM

# Usage :
#    bzcat /c/GIS/alsace.osm.bz2 | gawk -f tags.awk > toto
#    gnuplot
#      plot 'toto' index 0, '' index 1 with lines

/<node id=/ {
  innode=1
}

/<\/node>/ {
  innode=0
}

/<way id=/ {
  inway=1
}

/<\/way>/ {
  inway=0
}

/<tag k=/ {
  k=substr($2,3)
  v=substr($3,3)

  if (innode) {
    printf "%10s %10s\n", k, v
  }
}


