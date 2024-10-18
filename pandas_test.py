import pandas as pd
# Function to find the title of the most viewed video
def most_viewed_video(csv_file):
    # Read the CSV file into a DataFrame
    df = pd.read_csv(csv_file, encoding_errors='ignore')

    # Find the row with the maximum number of views
    max_views_row = df.loc[df['views'].idxmax()]

    # Extract the title of the most viewed video
    most_viewed_title = max_views_row['title']
    most_viewed_views = max_views_row['views']

    return most_viewed_title, most_viewed_views

# Example usage
if __name__ == "__main__":
    # Specify the path to the CSV file
    csv_file = 'files/USvideos.'
    
    files = ['files/USvideos.csv', 'files/CAvideos.csv', 'files/DEvideos.csv', 'files/FRvideos.csv', 'files/GBvideos.csv', 'files/MXvideos.csv', 'files/INvideos.csv', 'files/JPvideos.csv', 'files/KRvideos.csv', 'files/RUvideos.csv']
    # Get the most viewed video title
    for file in files:
        title, views = most_viewed_video(file)
        print(f"In the {file} file the title of the most viewed video is: {title} with {views} views")

