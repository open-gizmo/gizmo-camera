// Format a file size in KB (one decimal under 10 KB), in the language of the app
export const formatFileSize = ({ sizeBytes, locale }) => {
    const kilobytes = sizeBytes / 1024;
    const decimals = kilobytes < 10 ? 1 : 0;

    // Format the number in the current locale
    const formattedNumber = new Intl.NumberFormat(locale, {
        minimumFractionDigits: decimals,
        maximumFractionDigits: decimals
    }).format(kilobytes);

    // Return the size with its unit
    return `${formattedNumber} KB`;
};

// Build the file name of a snapshot from the time it was saved (snapshot-2026-11-09-14-30-05.jpg)
export const buildSnapshotFileName = (date = new Date()) => {
    const pad = value => String(value).padStart(2, '0');
    const datePart = `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
    const timePart = `${pad(date.getHours())}-${pad(date.getMinutes())}-${pad(date.getSeconds())}`;
    return `snapshot-${datePart}-${timePart}.jpg`;
};