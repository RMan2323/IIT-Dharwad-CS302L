#!/bin/bash

echo "       t0          t1          t2          t3          t4            total"
#echo "    0.303012    0.638929    0.269719    0.272858    0.168557        1.653076


for img in {7..1..-1}; do
    tmpfile=$(mktemp)
    
    # Run 5 times, save output to tmpfile
    for run in {1..5}; do
        ./a.out ../images/${img}.ppm ../images/${img}_out.ppm >> "$tmpfile"
    done
    echo -e "Image $img"
    cat $tmpfile
    # Use awk to sum columns and compute averages
    awk -v img="$img" '{
        t0 += $1; t1 += $2; t2 += $3; t3 += $4; t4 += $5; tot += $6;
    }
    END {
        printf "Averages:\n    %.6f    %.6f    %.6f    %.6f    %.6f        %.6f\n\n", t0/NR, t1/NR, t2/NR, t3/NR, t4/NR, tot/NR;
    }' "$tmpfile"

    rm "$tmpfile"
done
