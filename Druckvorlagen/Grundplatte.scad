/* Grundplatte
  Hält das Gewicht des Läufers. 
  In der Mitte ist ein Lager das das Gewicht
  aufnehmen kann.
  
  Die beiden flachen Enden halten es in der Pyramide
  
  Am besten mit Support drucken und die flachen
  Enden nachfeilen. Das ist einfacher als
  das Loch für das Lager nachzuarbeiten.
  
  Druck mit ABS dmit es ordentlich hält
  
  Mathias Moog, 2026, CC BY NC SA
*/

// Schöne Rundungen
$fn=60;

difference() {
  union() {
    // Grundplatte
    cylinder(h=7, d=58);
    // Flache Enden, angehoben
    translate([0,0,5]) cylinder(h=2, d=84);
  }
  // Auf die Breite von 20 mm zuschneiden
  translate([-50,10,-1]) cube([100,50,20]);  
  translate([-50,-60,-1]) cube([100,50,20]);
  // In der Mitte aussparen für Laufrad und Gummi
  translate([0,0,4]) cylinder(h=10, d=47);
  // Lager unten
  translate([0,0,-1]) cylinder(h=10, d=11);
  // Lager Haltering
  translate([0,0,3]) cylinder(h=10, d=12.5);
}  