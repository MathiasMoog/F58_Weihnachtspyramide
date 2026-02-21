/* Antrieb
  Oben wird die bisherige Achse eingesetzt. 
  Unten folgt das Antriebsrad mit Vertiefung und 
  der Stift für das Lager
  
  Am besten mit Support von unten drucken.
  Lässt sich gut nacharbeiten.
  
  Druck mit ABS dmit es ordentlich hält
  
  Mathias Moog, 2026, CC BY NC SA
*/

// Schöne Rundungen
$fn=60;

/* Torus aus Anleitung
  r1 - großer Radius
  r2 - kleiner Radius
*/
module torus(r1, r2) {
  rotate_extrude(convexity = 10)
    translate([r1, 0, 0])
        circle(r = r2);
}

d_laufrad = 40;
d_gummi = 3;
d_stange = 4.1;
d_stift = 7;

difference() {
  union() {
    // Stift oben
    cylinder(h=(10+4.5+13.8), d=d_stift);
    // Stift unten für Lager
    translate([0,0,-4]) cylinder(h=4, d=4.9);
    // Laufrad
    translate([0,0,2]) cylinder(h=6, d=d_laufrad);
  }
  // Konische Aufnahme
  translate([0,0,14.5]) cylinder(h=7, d1=1, d2=d_stange);
  // Senkrechte Führung
  translate([0,0,14.5+7]) cylinder(h=10, d=d_stange);
  // Vertiefung Gummi
  translate([0,0,5]) torus(d_laufrad/2,d_gummi/2);
}  

