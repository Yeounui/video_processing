# Surgical Changes: bad example - fixes email while rewriting unrelated validation.
def validate_user_too_much(user_data):
    """Validate user data."""
    email = user_data.get('email', '').strip()

    # Validate email
    if not email:
        raise ValueError("Email required")
    if '@' not in email or '.' not in email.split('@')[1]:
        raise ValueError("Invalid email")

    # Validate username
    username = user_data.get('username', '').strip()
    if not username:
        raise ValueError("Username required")
    if len(username) < 3:
        raise ValueError("Username too short")
    if not username.isalnum():
        raise ValueError("Username must be alphanumeric")

    return True


# Surgical Changes: good example - only guard the reported empty-email case.
def validate_user_surgical(user_data):
    # Check email format
    email = user_data.get('email', '')
    if not email or not email.strip():
        raise ValueError("Email required")

    # Basic email validation
    if '@' not in email:
        raise ValueError("Invalid email")

    # Check username
    if not user_data.get('username'):
        raise ValueError("Username required")

    return True


# Surgical Changes: good example - add logging while preserving local style.
import logging

logger = logging.getLogger(__name__)


def upload_file(file_path, destination):
    logger.info(f'Starting upload: {file_path}')
    try:
        with open(file_path, 'rb') as f:
            data = f.read()

        response = requests.post(destination, files={'file': data})

        if response.status_code == 200:
            logger.info(f'Upload successful: {file_path}')
            return True
        else:
            logger.error(f'Upload failed: {file_path}, status={response.status_code}')
            return False
    except Exception:
        logger.exception(f'Upload error: {file_path}')
        return False
