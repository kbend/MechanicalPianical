import mido
import csv

midi_file = mido.MidiFile("your-midi-file.mid")

absolute_time = 0.0

with open('midi_data.csv', 'w', newline='') as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(['time', 'type', 'note', 'velocity'])

    for msg in midi_file:
        absolute_time += msg.time
        if msg.type in ['note_on', 'note_off']:
            writer.writerow([f"{absolute_time:.3f}", msg.type, msg.note, msg.velocity])
