import pandas as pd
import multiprocessing

# Function to find the title of the most viewed video
def most_viewed_video(csv_file):
    # Read the CSV file into a DataFrame
    df = pd.read_csv(csv_file, encoding_errors='ignore')

    # Find the row with the maximum number of views
    max_views_row = df.loc[df['views'].idxmax()]

    # Extract the title of the most viewed video
    most_viewed_title = max_views_row['title']
    most_viewed_views = max_views_row['views']

    return (csv_file, most_viewed_title, most_viewed_views)

if __name__ == "__main__":
    # List of CSV files to process
    files = [
        'files/USvideos.csv', 
        'files/CAvideos.csv', 
        'files/DEvideos.csv', 
        'files/FRvideos.csv', 
        'files/GBvideos.csv', 
        'files/MXvideos.csv', 
        'files/INvideos.csv', 
        'files/JPvideos.csv', 
        'files/KRvideos.csv', 
        'files/RUvideos.csv'
    ]

    # Create a multiprocessing Pool
    with multiprocessing.Pool(processes=multiprocessing.cpu_count()) as pool:
        # Use the pool to map the function to the list of files
        results = pool.map(most_viewed_video, files)

    # Print the results
    for file, title, views in results:
        print(f"In the {file} file, the title of the most viewed video is: '{title}' with {views} views.")
